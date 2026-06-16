/*
 * vm.js - browser-adapted bytecode VM interpreter
 * Ported from vm/cli.ts for web execution
 */

class VirtualMachine {
  constructor() {
    this.globals = new Map();
    this.stack = [];
    this.heap = new Map();
    this.heapCounter = 1;
    this.instructions = [];
    this.labels = new Map();
    this.ip = 0;
    this.status = "idle";
    this.screenBuffer = new Array(64 * 64).fill(0);
    this.callStack = [];
    this.keyState = new Array(9).fill(0);
    this.stats = { loc: 0, instructionsExecuted: 0, maxMemory: 0 };
    this.sourceCode = null;
    this.error = null;
    this.inputDest = "";
    this.paramStack = [];
    this.outputCallback = null;
    this.inputCallback = null;
  }

  reset() {
    const saved = this.sourceCode;
    const outputCb = this.outputCallback;
    const inputCb = this.inputCallback;
    this.globals = new Map();
    this.stack = [];
    this.heap = new Map();
    this.heapCounter = 1;
    this.instructions = [];
    this.labels = new Map();
    this.ip = 0;
    this.status = "idle";
    this.screenBuffer = new Array(64 * 64).fill(0);
    this.callStack = [];
    this.keyState = new Array(9).fill(0);
    this.paramStack = [];
    this.error = null;
    this.stats = { loc: 0, instructionsExecuted: 0, maxMemory: 0 };
    this.sourceCode = saved;
    this.outputCallback = outputCb;
    this.inputCallback = inputCb;
  }

  load(code) {
    this.sourceCode = code;
    this.reset();
    this.sourceCode = code;

    let addr = 0;
    const lines = code.split("\n");

    for (let i = 0; i < lines.length; i++) {
      let line = lines[i].trim();
      if (!line || line.startsWith("//")) continue;
      const ci = line.indexOf("//");
      if (ci !== -1) line = line.slice(0, ci).trim();
      if (!line) continue;

      const parts = parseLine(line);
      if (parts.length === 0) continue;

      if (parts[0].toUpperCase() === "LABEL") {
        this.labels.set(parts[1], addr);
      } else {
        this.instructions.push({ addr, parts, sourceLine: i + 1 });
        addr++;
      }
    }

    this.stats.loc = this.instructions.length;
  }

  calcMemory() {
    let usage = this.globals.size;
    for (const f of this.callStack) usage += f.locals.size;
    for (const [, obj] of this.heap) usage += obj.length;
    if (usage > this.stats.maxMemory) this.stats.maxMemory = usage;
  }

  executeStep() {
    if (this.ip >= this.instructions.length) {
      this.status = "finished";
      return false;
    }

    const inst = this.instructions[this.ip];
    const op = inst.parts[0].toUpperCase();
    const args = inst.parts.slice(1);

    try {
      this.stats.instructionsExecuted++;
      this.executeInstruction(op, args, inst.sourceLine);

      if (
        ["VAR", "ARR", "LIST", "LIST_ADD", "GOSUB", "RETURN"].includes(op) ||
        this.stats.instructionsExecuted % 1000 === 0
      ) this.calcMemory();

      if (
        this.status !== "waiting_input" &&
        this.status !== "error" &&
        !["GOTO", "IF", "IFFALSE", "GOSUB", "RETURN", "FUNCTION", "END_FUNCTION"].includes(op)
      ) this.ip++;

    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      this.error = `ip=${this.ip} line ${inst.sourceLine}: ${msg}`;
      this.status = "error";
      return false;
    }

    return this.status === "running";
  }

  executeInstruction(op, args, srcLine) {
    const get = (i) => this.getValue(args[i]);
    const num = (i) => {
      const v = get(i);
      if (typeof v !== "number") throw new Error(`expected number for arg[${i}], got string`);
      return v;
    };
    const set = (i, v) => this.setValue(args[i], v);
    const bool = (b) => b ? 1 : 0;
    const jump = (label) => {
      const addr = this.labels.get(label);
      if (addr === undefined) throw new Error(`unknown label: ${label}`);
      this.ip = addr;
    };

    switch (op) {
      case "VAR":
        if (this.callStack.length > 0)
          this.callStack[this.callStack.length - 1].locals.set(args[0], 0);
        else
          this.globals.set(args[0], 0);
        break;

      case "ASSIGN": set(1, get(0)); break;
      case "ADD": {
        const v1 = get(0);
        const v2 = get(1);
        if (typeof v1 === "string" || typeof v2 === "string") {
          set(2, String(v1) + String(v2));
        } else {
          set(2, v1 + v2);
        }
        break;
      }
      case "SUB": set(2, num(0) - num(1)); break;
      case "MUL": set(2, num(0) * num(1)); break;
      case "DIV": {
        const d = num(1);
        if (d === 0) throw new Error("division by zero");
        set(2, num(0) / d);
        break;
      }
      case "MOD": set(2, num(0) % num(1)); break;
      case "POW": set(2, Math.pow(num(0), num(1))); break;

      case "AND": set(2, bool(!!(get(0)) && !!(get(1)))); break;
      case "OR": set(2, bool(!!(get(0)) || !!(get(1)))); break;
      case "NOT": set(1, bool(!get(0))); break;
      case "XOR": set(2, bool(!!get(0) !== !!get(1))); break;

      case "EQ": set(2, bool(get(0) === get(1))); break;
      case "NEQ": set(2, bool(get(0) !== get(1))); break;
      case "GT": set(2, bool(num(0) > num(1))); break;
      case "GTE": set(2, bool(num(0) >= num(1))); break;
      case "LT": set(2, bool(num(0) < num(1))); break;
      case "LTE": set(2, bool(num(0) <= num(1))); break;

      case "LABEL": break;
      case "GOTO": jump(args[0]); break;
      case "IF": if (get(0)) jump(args[2]); else this.ip++; break;
      case "IFFALSE": if (!get(0)) jump(args[2]); else this.ip++; break;

      case "FUNCTION": {
        let depth = 1;
        while (depth > 0) {
          this.ip++;
          if (this.ip >= this.instructions.length) break;
          const nop = this.instructions[this.ip].parts[0].toUpperCase();
          if (nop === "FUNCTION") depth++;
          if (nop === "END_FUNCTION") depth--;
        }
        this.ip++;
        break;
      }
      case "END_FUNCTION": this.doReturn(); break;

      case "PARAM":
        this.paramStack.push(get(0));
        break;
      case "PARAM_GET":
        if (this.paramStack.length === 0) throw new Error("parameter stack empty");
        set(0, this.paramStack.pop());
        break;

      case "GOSUB": {
        const addr = this.labels.get(args[0]);
        if (addr === undefined) throw new Error(`unknown function: ${args[0]}`);
        this.callStack.push({
          returnAddr: this.ip + 1,
          returnVar: args[1] ?? null,
          locals: new Map(),
        });
        this.ip = addr;
        break;
      }
      case "RETURN":
        this.doReturn(args.length > 0 ? get(0) : 0);
        break;

      case "ARR": {
        const id = this.heapCounter++;
        this.heap.set(id, new Array(Math.floor(num(0))).fill(0));
        set(1, id);
        break;
      }
      case "ARR_GET": {
        const arr = this.getHeap(args[0]);
        const idx = Math.floor(num(1));
        if (idx < 0 || idx >= arr.length) throw new Error(`array index out of bounds: ${idx}`);
        set(2, arr[idx]);
        break;
      }
      case "ARR_SET": {
        const arr = this.getHeap(args[0]);
        const idx = Math.floor(num(1));
        if (idx < 0 || idx >= arr.length) throw new Error(`array index out of bounds: ${idx}`);
        arr[idx] = get(2);
        break;
      }

      case "LIST": {
        const id = this.heapCounter++;
        this.heap.set(id, []);
        set(0, id);
        break;
      }
      case "LIST_ADD": this.getHeap(args[0]).push(get(1)); break;
      case "LIST_GET": {
        const list = this.getHeap(args[0]);
        const idx = Math.floor(num(1));
        if (idx < 0 || idx >= list.length) throw new Error(`list index out of bounds: ${idx}`);
        set(2, list[idx]);
        break;
      }

      case "PRINT": {
        const raw = args[0];
        const out = raw.startsWith('"') ? raw.slice(1, -1) : String(get(0));
        if (this.outputCallback) {
          this.outputCallback(out);
        } else {
          console.log(out);
        }
        break;
      }
      case "INPUT":
        this.inputDest = args[0];
        this.status = "waiting_input";
        break;

      case "PIXEL": {
        const x = Math.floor(num(0)), y = Math.floor(num(1));
        if (x >= 0 && x < 64 && y >= 0 && y < 64)
          this.screenBuffer[y * 64 + x] = get(2) ? 1 : 0;
        break;
      }
      case "SLEEP": break;
      case "KEY": {
        const k = Math.floor(num(0));
        if (k < 0 || k >= this.keyState.length) throw new Error(`invalid key code: ${k}`);
        set(1, this.keyState[k] ? 1 : 0);
        break;
      }
      case "TIME":
        set(0, Date.now());
        break;

      default:
        throw new Error(`unknown instruction: ${op}`);
    }
  }

  getHeap(ref) {
    const id = this.getValue(ref);
    if (typeof id !== "number") throw new Error(`heap ref must be a number`);
    const obj = this.heap.get(id);
    if (!obj) throw new Error(`invalid heap reference: ${id}`);
    return obj;
  }

  getValue(arg) {
    const n = Number(arg);
    if (!isNaN(n) && arg.trim() !== "") return n;
    if (arg === "true") return 1;
    if (arg === "false") return 0;
    if (arg.startsWith('"')) return arg.slice(1, -1);

    if (this.callStack.length > 0) {
      const locals = this.callStack[this.callStack.length - 1].locals;
      if (locals.has(arg)) return locals.get(arg);
    }
    if (this.globals.has(arg)) return this.globals.get(arg);
    throw new Error(`undefined variable: ${arg}`);
  }

  setValue(dest, val) {
    if (this.callStack.length > 0) {
      const locals = this.callStack[this.callStack.length - 1].locals;
      if (locals.has(dest)) { locals.set(dest, val); return; }
    }
    if (this.globals.has(dest)) { this.globals.set(dest, val); return; }
    if (this.callStack.length > 0)
      this.callStack[this.callStack.length - 1].locals.set(dest, val);
    else
      this.globals.set(dest, val);
  }

  doReturn(val = 0) {
    if (this.callStack.length === 0) { this.status = "finished"; return; }
    const frame = this.callStack.pop();
    this.ip = frame.returnAddr;
    if (frame.returnVar) this.setValue(frame.returnVar, val);
  }
}

function parseLine(line) {
  const parts = [];
  let cur = "";
  let inStr = false;
  for (const ch of line) {
    if (ch === '"') { inStr = !inStr; cur += ch; }
    else if (ch === " " && !inStr) { if (cur) { parts.push(cur); cur = ""; } }
    else cur += ch;
  }
  if (cur) parts.push(cur);
  return parts;
}

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { VirtualMachine };
}
