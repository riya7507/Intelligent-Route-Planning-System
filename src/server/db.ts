import initSqlJs, { Database } from 'sql.js';
import fs from 'fs';
import path from 'path';

let db: Database | null = null;
const DB_PATH = path.resolve(process.cwd(), 'database.sqlite');

export async function initDatabase(): Promise<Database> {
  if (db) return db;

  const SQL = await initSqlJs();

  if (fs.existsSync(DB_PATH)) {
    try {
      const fileBuffer = fs.readFileSync(DB_PATH);
      db = new SQL.Database(fileBuffer);
    } catch (e) {
      console.warn('Could not read existing database.sqlite, creating fresh database', e);
      db = new SQL.Database();
    }
  } else {
    db = new SQL.Database();
  }

  // Schema initialization
  db.run(`
    CREATE TABLE IF NOT EXISTS route_requests (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      source INTEGER NOT NULL,
      destination INTEGER NOT NULL,
      algorithm TEXT NOT NULL,
      threads INTEGER NOT NULL,
      reachable INTEGER NOT NULL,
      distance INTEGER NOT NULL,
      algorithm_ms REAL NOT NULL,
      path_ms REAL NOT NULL,
      path_length INTEGER NOT NULL,
      validated INTEGER NOT NULL,
      created_at TEXT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS benchmark_results (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      experiment TEXT NOT NULL,
      target_nodes INTEGER NOT NULL,
      actual_nodes INTEGER NOT NULL,
      actual_arcs INTEGER NOT NULL,
      algorithm TEXT NOT NULL,
      threads INTEGER NOT NULL,
      repetition INTEGER NOT NULL,
      time_ms REAL NOT NULL,
      distance INTEGER,
      speedup REAL,
      efficiency REAL,
      created_at TEXT NOT NULL
    );

    CREATE TABLE IF NOT EXISTS correctness_runs (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      suite_name TEXT NOT NULL,
      total_tests INTEGER NOT NULL,
      passed_tests INTEGER NOT NULL,
      failed_tests INTEGER NOT NULL,
      status TEXT NOT NULL,
      details TEXT,
      created_at TEXT NOT NULL
    );
  `);

  saveDatabase();
  return db;
}

export function saveDatabase(): void {
  if (!db) return;
  try {
    const data = db.export();
    const buffer = Buffer.from(data);
    fs.writeFileSync(DB_PATH, buffer);
  } catch (err) {
    console.error('Error saving SQLite database to disk:', err);
  }
}

export interface RouteLog {
  source: number;
  destination: number;
  algorithm: string;
  threads: number;
  reachable: boolean;
  distance: number;
  algorithmMs: number;
  pathMs: number;
  pathLength: number;
  validated: boolean;
}

export function logRouteRequest(log: RouteLog): void {
  if (!db) return;
  try {
    db.run(
      `INSERT INTO route_requests 
        (source, destination, algorithm, threads, reachable, distance, algorithm_ms, path_ms, path_length, validated, created_at)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
      [
        log.source,
        log.destination,
        log.algorithm,
        log.threads,
        log.reachable ? 1 : 0,
        log.distance,
        log.algorithmMs,
        log.pathMs,
        log.pathLength,
        log.validated ? 1 : 0,
        new Date().toISOString()
      ]
    );
    saveDatabase();
  } catch (err) {
    console.error('Failed to log route request:', err);
  }
}

export function getRecentRouteLogs(limit = 15): any[] {
  if (!db) return [];
  try {
    const res = db.exec(`SELECT * FROM route_requests ORDER BY id DESC LIMIT ${limit}`);
    if (res.length === 0) return [];
    const columns = res[0].columns;
    return res[0].values.map((row) => {
      const obj: any = {};
      columns.forEach((col, idx) => {
        obj[col] = row[idx];
      });
      return obj;
    });
  } catch (e) {
    console.error('Error fetching recent route logs:', e);
    return [];
  }
}
