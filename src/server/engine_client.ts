import { spawn, ChildProcess } from 'child_process';
import readline from 'readline';
import path from 'path';
import fs from 'fs';

interface EngineRequest {
  cmd: string;
  source?: number;
  destination?: number;
  algorithm?: string;
  threads?: number;
  [key: string]: any;
}

interface EngineResponse {
  status: string;
  error?: string;
  [key: string]: any;
}

class EngineClient {
  private process: ChildProcess | null = null;
  private rl: readline.Interface | null = null;
  private pendingResolvers: Array<{
    resolve: (res: EngineResponse) => void;
    reject: (err: any) => void;
  }> = [];
  private graphInfoCache: any = null;

  constructor() {
    this.startEngine();
  }

  private startEngine() {
    const engineBin = path.resolve(process.cwd(), 'engine/bin/engine_server');
    const graphPath = path.resolve(process.cwd(), 'engine/data/USA-road-d.BAY.gr');
    const coPath = path.resolve(process.cwd(), 'engine/data/USA-road-d.BAY.co');

    if (!fs.existsSync(engineBin)) {
      console.warn('[EngineClient] engine_server binary not found at', engineBin);
      return;
    }

    try {
      this.process = spawn(engineBin, [graphPath, coPath], {
        stdio: ['pipe', 'pipe', 'inherit'],
      });

      if (!this.process.stdout || !this.process.stdin) {
        console.error('[EngineClient] Process streams are null');
        return;
      }

      this.rl = readline.createInterface({
        input: this.process.stdout,
        terminal: false,
      });

      this.rl.on('line', (line) => {
        const trimmed = line.trim();
        if (!trimmed) return;
        try {
          const parsed = JSON.parse(trimmed);
          const next = this.pendingResolvers.shift();
          if (next) {
            next.resolve(parsed);
          }
        } catch (e) {
          console.error('[EngineClient] Failed to parse engine stdout line:', line, e);
          const next = this.pendingResolvers.shift();
          if (next) {
            next.reject(new Error('Malformed JSON response from engine'));
          }
        }
      });

      this.process.on('exit', (code, signal) => {
        console.warn(`[EngineClient] Engine process exited with code ${code}, signal ${signal}`);
        this.process = null;
        this.rl = null;
        while (this.pendingResolvers.length > 0) {
          const next = this.pendingResolvers.shift();
          if (next) next.reject(new Error('Engine process exited unexpectedly'));
        }
      });
    } catch (err) {
      console.error('[EngineClient] Error spawning engine:', err);
    }
  }

  public async sendCommand(req: EngineRequest): Promise<EngineResponse> {
    if (!this.process || this.process.killed || !this.process.stdin) {
      this.startEngine();
    }

    if (!this.process || !this.process.stdin) {
      throw new Error('Engine process is not running');
    }

    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        reject(new Error('Engine request timed out after 30 seconds'));
      }, 30000);

      this.pendingResolvers.push({
        resolve: (data) => {
          clearTimeout(timeout);
          resolve(data);
        },
        reject: (err) => {
          clearTimeout(timeout);
          reject(err);
        },
      });

      const line = JSON.stringify(req) + '\n';
      this.process!.stdin!.write(line);
    });
  }

  public async getInfo(): Promise<any> {
    if (this.graphInfoCache) return this.graphInfoCache;
    try {
      const res = await this.sendCommand({ cmd: 'info' });
      if (res.status === 'ok') {
        this.graphInfoCache = res;
      }
      return res;
    } catch (err) {
      return {
        status: 'error',
        error: (err as any).message || 'Failed to query engine info',
        nodes: 321270,
        arcs: 800172,
        avg_degree: 2.49,
      };
    }
  }

  public async planRoute(
    source: number,
    destination: number,
    algorithm: string = 'omp-scan',
    threads: number = 2
  ): Promise<any> {
    return this.sendCommand({
      cmd: 'route',
      source,
      destination,
      algorithm,
      threads,
    });
  }
}

export const engineClient = new EngineClient();

export function parseCsvFile(filePath: string): any[] {
  if (!fs.existsSync(filePath)) return [];
  const content = fs.readFileSync(filePath, 'utf-8');
  const lines = content.trim().split('\n');
  if (lines.length < 2) return [];

  const headers = lines[0].split(',').map((h) => h.trim());
  const rows: any[] = [];

  for (let i = 1; i < lines.length; i++) {
    const parts = lines[i].split(',').map((p) => p.trim());
    if (parts.length === headers.length) {
      const row: any = {};
      headers.forEach((h, idx) => {
        const val = parts[idx];
        const num = Number(val);
        row[h] = isNaN(num) || val === '' ? val : num;
      });
      rows.push(row);
    }
  }

  return rows;
}
