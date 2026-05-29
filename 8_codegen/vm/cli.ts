#!/usr/bin/env bun
/*
 * vm.ts — standalone bytecode VM interpreter, single-file Bun script
 *
 * Usage:
 *   bun vm.ts <file>           Run, print output + stats
 *   bun vm.ts <file> --verbose Also print system events
 *   bun vm.ts <file> --screen  Render the 64×64 pixel framebuffer
 *   bun vm.ts --help
 */

// ─── ANSI ─────────────────────────────────────────────────────────────────────

const R  = "\x1b[0m";
const B  = "\x1b[1m";
const DM = "\x1b[2m";
const RD = "\x1b[31m";
const GR = "\x1b[32m";
const YL = "\x1b[33m";
const CY = "\x1b[36m";
const GY = "\x1b[90m";

// ─── Types ────────────────────────────────────────────────────────────────────

type VMStatus  = "idle" | "running" | "waiting_input" | "finished" | "error";
type VMValue   = number | string;
type LogType   = "info" | "error" | "system" | "input";

interface Instruction {
  addr:       number;
  parts:      string[];
  sourceLine: number;
}

interface CallFrame {
  returnAddr: number;
  returnVar:  string | null;
  locals:     Map<string, VMValue>;
}

interface VMStats {
  loc:                 number;
  instructionsExecuted: number;
  maxMemory:           number;
}

// ─── VirtualMachine ───────────────────────────────────────────────────────────

class VirtualMachine {
  globals      = new Map<string, VMValue>();
  stack:       VMValue[] = [];
  heap         = new Map<number, VMValue[]>();
  heapCounter  = 1;

  instructions: Instruction[] = [];
  labels        = new Map<string, number>();
  ip            = 0;

  status:       VMStatus = "idle";
  screenBuffer: number[] = new Array(64 * 64).fill(0);
  callStack:    CallFrame[] = [];
  keyState:     number[] = new Array(9).fill(0);
  stats:        VMStats = { loc: 0, instructionsExecuted: 0, maxMemory: 0 };

  sourceCode:   string | null = null;
  error:        string | null = null;
  inputDest:    string = "";

  private paramStack: VMValue[] = [];

  // ── Lifecycle ──────────────────────────────────────────────────────────────

  reset(): void {
    const saved = this.sourceCode;
    this.globals      = new Map();
    this.stack        = [];
    this.heap         = new Map();
    this.heapCounter  = 1;
    this.instructions = [];
    this.labels       = new Map();
    this.ip           = 0;
    this.status       = "idle";
    this.screenBuffer = new Array(64 * 64).fill(0);
    this.callStack    = [];
    this.keyState     = new Array(9).fill(0);
    this.paramStack   = [];
    this.error        = null;
    this.stats        = { loc: 0, instructionsExecuted: 0, maxMemory: 0 };
    this.sourceCode   = saved;
  }

  load(code: string): void {
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

  // ── Memory ─────────────────────────────────────────────────────────────────

  calcMemory(): void {
    let usage = this.globals.size;
    for (const f of this.callStack) usage += f.locals.size;
    for (const [, obj] of this.heap) usage += obj.length;
    if (usage > this.stats.maxMemory) this.stats.maxMemory = usage;
  }

  // ── Execution ──────────────────────────────────────────────────────────────

  executeStep(): boolean {
    if (this.ip >= this.instructions.length) {
      this.status = "finished";
      return false;
    }

    const inst = this.instructions[this.ip];
    const op   = inst.parts[0].toUpperCase();
    const args = inst.parts.slice(1);

    try {
      this.stats.instructionsExecuted++;
      this.executeInstruction(op, args, inst.sourceLine);

      if (
        ["VAR","ARR","LIST","LIST_ADD","GOSUB","RETURN"].includes(op) ||
        this.stats.instructionsExecuted % 1000 === 0
      ) this.calcMemory();

      if (
        this.status !== "waiting_input" &&
        this.status !== "error" &&
        !["GOTO","IF","IFFALSE","GOSUB","RETURN","FUNCTION","END_FUNCTION"].includes(op)
      ) this.ip++;

    } catch (e: unknown) {
      const msg = e instanceof Error ? e.message : String(e);
      this.error  = `ip=${this.ip} line ${inst.sourceLine}: ${msg}`;
      this.status = "error";
      return false;
    }

    return this.status === "running";
  }

  private executeInstruction(op: string, args: string[], srcLine: number): void {
    const get  = (i: number): VMValue => this.getValue(args[i]);
    const num  = (i: number): number  => {
      const v = get(i);
      if (typeof v !== "number") throw new Error(`expected number for arg[${i}], got string`);
      return v;
    };
    const set  = (i: number, v: VMValue) => this.setValue(args[i], v);
    const bool = (b: boolean): number => b ? 1 : 0;
    const jump = (label: string) => {
      const addr = this.labels.get(label);
      if (addr === undefined) throw new Error(`unknown label: ${label}`);
      this.ip = addr;
    };

    switch (op) {

      // ── Variables ──────────────────────────────────────────────────────────
      case "VAR":
        if (this.callStack.length > 0)
          this.callStack[this.callStack.length - 1].locals.set(args[0], 0);
        else
          this.globals.set(args[0], 0);
        break;

      // ── Assignment / arithmetic ────────────────────────────────────────────
      case "ASSIGN": set(1, get(0)); break;
      case "ADD": {
        const v1 = get(0);
        const v2 = get(1);
        if (typeof v1 === "string" || typeof v2 === "string") {
            set(2, String(v1) + String(v2));
        } else {
            set(2, (v1 as number) + (v2 as number));
        }
        break;
      }
      case "SUB":    set(2, num(0) - num(1)); break;
      case "MUL":    set(2, num(0) * num(1)); break;
      case "DIV": {
        const d = num(1);
        if (d === 0) throw new Error("division by zero");
        set(2, num(0) / d); break;
      }
      case "MOD": set(2, num(0) % num(1)); break;
      case "POW": set(2, Math.pow(num(0), num(1))); break;

      // ── Logic ──────────────────────────────────────────────────────────────
      case "AND": set(2, bool(!!(get(0)) && !!(get(1)))); break;
      case "OR":  set(2, bool(!!(get(0)) || !!(get(1)))); break;
      case "NOT": set(1, bool(!get(0))); break;
      case "XOR": set(2, bool(!!get(0) !== !!get(1))); break;

      // ── Comparison ─────────────────────────────────────────────────────────
      case "EQ":  set(2, bool(get(0) === get(1))); break;
      case "NEQ": set(2, bool(get(0) !== get(1))); break;
      case "GT":  set(2, bool(num(0) >  num(1))); break;
      case "GTE": set(2, bool(num(0) >= num(1))); break;
      case "LT":  set(2, bool(num(0) <  num(1))); break;
      case "LTE": set(2, bool(num(0) <= num(1))); break;

      // ── Control flow ───────────────────────────────────────────────────────
      case "LABEL": break;
      case "GOTO":  jump(args[0]); break;
      case "IF":      if ( get(0))  jump(args[2]); else this.ip++; break;
      case "IFFALSE": if (!get(0))  jump(args[2]); else this.ip++; break;

      // ── Functions ──────────────────────────────────────────────────────────
      case "FUNCTION": {
        let depth = 1;
        while (depth > 0) {
          this.ip++;
          if (this.ip >= this.instructions.length) break;
          const nop = this.instructions[this.ip].parts[0].toUpperCase();
          if (nop === "FUNCTION")     depth++;
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
        set(0, this.paramStack.pop()!);
        break;

      case "GOSUB": {
        const addr = this.labels.get(args[0]);
        if (addr === undefined) throw new Error(`unknown function: ${args[0]}`);
        this.callStack.push({
          returnAddr: this.ip + 1,
          returnVar:  args[1] ?? null,
          locals:     new Map(),
        });
        this.ip = addr;
        break;
      }
      case "RETURN":
        this.doReturn(args.length > 0 ? get(0) : 0);
        break;

      // ── Arrays ─────────────────────────────────────────────────────────────
      case "ARR": {
        const id = this.heapCounter++;
        this.heap.set(id, new Array(Math.floor(num(0))).fill(0));
        set(1, id); break;
      }
      case "ARR_GET": {
        const arr = this.getHeap(args[0]);
        const idx = Math.floor(num(1));
        if (idx < 0 || idx >= arr.length) throw new Error(`array index out of bounds: ${idx}`);
        set(2, arr[idx]); break;
      }
      case "ARR_SET": {
        const arr = this.getHeap(args[0]);
        const idx = Math.floor(num(1));
        if (idx < 0 || idx >= arr.length) throw new Error(`array index out of bounds: ${idx}`);
        arr[idx] = get(2); break;
      }

      // ── Lists ──────────────────────────────────────────────────────────────
      case "LIST": {
        const id = this.heapCounter++;
        this.heap.set(id, []);
        set(0, id); break;
      }
      case "LIST_ADD": this.getHeap(args[0]).push(get(1)); break;
      case "LIST_GET": {
        const list = this.getHeap(args[0]);
        const idx  = Math.floor(num(1));
        if (idx < 0 || idx >= list.length) throw new Error(`list index out of bounds: ${idx}`);
        set(2, list[idx]); break;
      }

      // ── I/O ────────────────────────────────────────────────────────────────
      case "PRINT": {
        const raw = args[0];
        const out = raw.startsWith('"') ? raw.slice(1, -1) : String(get(0));
        process.stdout.write(out + "\n");
        break;
      }
      case "INPUT":
        this.inputDest = args[0];
        this.status    = "waiting_input";
        break;

      // ── Graphics (no-op display in CLI; buffer kept for --screen) ──────────
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
        set(1, this.keyState[k] ? 1 : 0); break;
      }
      case "TIME":
        set(0, Date.now()); break;

      default:
        throw new Error(`unknown instruction: ${op}`);
    }
  }

  // ── Helpers ────────────────────────────────────────────────────────────────

  private getHeap(ref: string): VMValue[] {
    const id = this.getValue(ref);
    if (typeof id !== "number") throw new Error(`heap ref must be a number`);
    const obj = this.heap.get(id);
    if (!obj) throw new Error(`invalid heap reference: ${id}`);
    return obj;
  }

  getValue(arg: string): VMValue {
    const n = Number(arg);
    if (!isNaN(n) && arg.trim() !== "") return n;
    if (arg === "true")  return 1;
    if (arg === "false") return 0;
    if (arg.startsWith('"')) return arg.slice(1, -1);

    if (this.callStack.length > 0) {
      const locals = this.callStack[this.callStack.length - 1].locals;
      if (locals.has(arg)) return locals.get(arg)!;
    }
    if (this.globals.has(arg)) return this.globals.get(arg)!;
    throw new Error(`undefined variable: ${arg}`);
  }

  setValue(dest: string, val: VMValue): void {
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

  private doReturn(val: VMValue = 0): void {
    if (this.callStack.length === 0) { this.status = "finished"; return; }
    const frame = this.callStack.pop()!;
    this.ip = frame.returnAddr;
    if (frame.returnVar) this.setValue(frame.returnVar, val);
  }
}

// ─── Parser (module-level, shared) ────────────────────────────────────────────

function parseLine(line: string): string[] {
  const parts: string[] = [];
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

// ─── CLI ──────────────────────────────────────────────────────────────────────

function printHelp(): void {
  console.log(`
${B}${CY}VM${R} - bytecode interpreter

${B}Usage:${R}
  bun vm.ts <file> [options]

${B}Options:${R}
  --verbose   Print system/diagnostic messages
  --screen    Render the 64×64 pixel framebuffer after execution
  --help, -h  Show this help
`);
}

function renderScreen(buf: number[]): void {
  console.log(`\n${DM}── Screen (64×64) ──────────────────────────────${R}`);
  let lastRow = 0;
  for (let y = 0; y < 64; y++)
    for (let x = 0; x < 64; x++)
      if (buf[y * 64 + x]) lastRow = y;
  for (let y = 0; y <= lastRow; y++)
    console.log(Array.from({ length: 64 }, (_, x) => buf[y * 64 + x] ? "█" : " ").join(""));
}

function printStats(vm: VirtualMachine, elapsedMs: number, filename: string, ok: boolean): void {
  const ips    = elapsedMs > 0
    ? Math.round(vm.stats.instructionsExecuted / elapsedMs * 1000).toLocaleString()
    : "∞";
  const status = ok ? `${GR}${B}OK${R}` : `${RD}${B}ERROR${R}`;
  const bar    = `${DM}${"─".repeat(50)}${R}`;
  const rows: [string, string][] = [
    ["File",          filename],
    ["Status",        status],
    ["Source lines",  vm.stats.loc.toLocaleString()],
    ["Instructions",  vm.stats.instructionsExecuted.toLocaleString()],
    ["Peak memory",   `${vm.stats.maxMemory.toLocaleString()} slots`],
    ["Elapsed",       `${elapsedMs.toFixed(2)} ms`],
    ["Throughput",    `${ips} instr/s`],
    ["Call depth",    vm.callStack.length.toLocaleString()],
    ["Heap objects",  vm.heap.size.toLocaleString()],
  ];
  const w = Math.max(...rows.map(([l]) => l.length)) + 2;
  console.log(`\n${bar}`);
  console.log(`${B}  Execution summary${R}`);
  console.log(bar);
  for (const [label, value] of rows)
    console.log(`  ${GY}${label.padEnd(w)}${R}${value}`);
  console.log(bar + "\n");
}

// ─── Input reader (Bun synchronous stdin) ─────────────────────────────────────

function readLineSync(): string {
  // Bun exposes a synchronous stdin reader via Bun.stdin.stream()
  // We use a small buffer read loop to get one line without async/await,
  // keeping the run loop simple and avoiding Promise overhead.
  const buf = Buffer.alloc(4096);
  let result = "";
  while (true) {
    const n = require("fs").readSync(0 /* stdin fd */, buf, 0, 1, null);
    if (n === 0) break;
    const ch = buf.toString("utf8", 0, 1);
    if (ch === "\n") break;
    result += ch;
  }
  return result.replace(/\r$/, "");
}

// ─── Main ─────────────────────────────────────────────────────────────────────

const argv = process.argv.slice(2);

if (argv.length === 0 || argv.includes("--help") || argv.includes("-h")) {
  printHelp();
  process.exit(0);
}

const filePath   = argv.find(a => !a.startsWith("-")) ?? "";
const optVerbose = argv.includes("--verbose");
const optScreen  = argv.includes("--screen");

if (!filePath) {
  console.error(`${RD}Error: no file specified.${R}`);
  process.exit(1);
}

const file = Bun.file(filePath);
if (!(await file.exists())) {
  console.error(`${RD}Error: file not found — ${filePath}${R}`);
  process.exit(1);
}

const source   = await file.text();
const filename = filePath.split(/[\\/]/).pop()!;

console.log(`\n${B}▶  ${filename}${R}`);
console.log(`${DM}${"─".repeat(50)}${R}`);

const vm = new VirtualMachine();

try {
  vm.load(source);
} catch (e: unknown) {
  console.error(`${RD}Compile error: ${e instanceof Error ? e.message : e}${R}`);
  process.exit(1);
}

if (optVerbose)
  console.log(`${GY}   Loaded ${vm.stats.loc} instructions.${R}`);

// ── Run loop ──────────────────────────────────────────────────────────────────

vm.status = "running";
const t0  = performance.now();

const CHUNK = 10_000;

outer: while (true) {
  for (let i = 0; i < CHUNK; i++) {
    if (vm.status !== "running") break;
    vm.executeStep();
  }

  switch (vm.status as string) {
    case "finished":
      break outer;

    case "error":
      break outer;

    case "waiting_input": {
      process.stdout.write(`${YL}? ${R}`);
      const line   = readLineSync();
      const parsed = parseFloat(line);
      vm.setValue(vm.inputDest, isNaN(parsed) ? line : parsed);
      vm.ip++;
      vm.status = "running";
      break;
    }

    // still "running" after the chunk — continue
  }
}

const elapsed = performance.now() - t0;
vm.calcMemory();

// ── Post-run output ───────────────────────────────────────────────────────────

if (vm.status === "error" && vm.error)
  console.error(`\n${RD}Runtime error: ${vm.error}${R}`);

if (optScreen) renderScreen(vm.screenBuffer);

const ok = (vm.status as string) === "finished";
printStats(vm, elapsed, filename, ok);

process.exit(ok ? 0 : 1);
