// Minimal MCP stdio server that lets Claude run Codex jobs via `codex exec`.
// No dependencies. Jobs run in the background; Claude polls for results.
import { spawn } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import readline from 'node:readline';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const JOBS = path.join(HERE, 'jobs');
fs.mkdirSync(JOBS, { recursive: true });
const DEFAULT_CWD = path.resolve(HERE, '..', '..');
// Prefer the Codex installed next to the node running this bridge (nvm4w global),
// then the classic %APPDATA%\npm location. CODEX_JS env var overrides both.
const CODEX_CANDIDATES = [
  process.env.CODEX_JS,
  path.join(path.dirname(process.execPath), 'node_modules', '@openai', 'codex', 'bin', 'codex.js'),
  path.join(process.env.APPDATA || '', 'npm', 'node_modules', '@openai', 'codex', 'bin', 'codex.js'),
].filter(Boolean);
const CODEX_JS = CODEX_CANDIDATES.find((p) => fs.existsSync(p)) || CODEX_CANDIDATES[0];
const jobs = new Map();

function send(msg) { process.stdout.write(JSON.stringify(msg) + '\n'); }
function text(s) { return { content: [{ type: 'text', text: s }] }; }
function tail(s, n = 12000) { return s.length > n ? '...[truncated]...\n' + s.slice(-n) : s; }

function startJob({ prompt, sandbox = 'read-only', model, cwd, resume_session }) {
  const id = new Date().toISOString().replace(/[:.]/g, '-') + '-' + Math.random().toString(36).slice(2, 6);
  const dir = path.join(JOBS, id); fs.mkdirSync(dir);
  const lastMsg = path.join(dir, 'last-message.txt');
  const args = [CODEX_JS, 'exec'];
  if (resume_session) args.push('resume', resume_session);
  args.push('--skip-git-repo-check', '-o', lastMsg);
  if (!resume_session) args.push('-C', cwd || DEFAULT_CWD, '-s', sandbox);
  if (model) args.push('-m', model);
  args.push('-');
  fs.writeFileSync(path.join(dir, 'prompt.txt'), prompt);
  const log = fs.openSync(path.join(dir, 'output.log'), 'a');
  const child = spawn(process.execPath, args, { cwd: cwd || DEFAULT_CWD, stdio: ['pipe', log, log], windowsHide: true });
  const job = { id, dir, lastMsg, status: 'running', started: new Date().toISOString(), exitCode: null };
  child.on('exit', (code) => { job.status = code === 0 ? 'done' : 'failed'; job.exitCode = code; job.finished = new Date().toISOString(); fs.closeSync(log); });
  child.on('error', (e) => { job.status = 'failed'; job.error = String(e); });
  child.stdin.end(prompt);
  job.child = child; jobs.set(id, job);
  return job;
}

function jobStatus(id) {
  const job = jobs.get(id);
  const dir = path.join(JOBS, id);
  if (!job && !fs.existsSync(dir)) return `No job ${id}`;
  const out = fs.existsSync(path.join(dir, 'output.log')) ? fs.readFileSync(path.join(dir, 'output.log'), 'utf8') : '';
  const sid = (out.match(/session id:\s*([0-9a-f-]+)/i) || [])[1] || 'unknown';
  const last = fs.existsSync(path.join(dir, 'last-message.txt')) ? fs.readFileSync(path.join(dir, 'last-message.txt'), 'utf8') : '';
  const status = job ? job.status : (last ? 'done' : 'unknown (bridge restarted)');
  let s = `job: ${id}\nstatus: ${status}${job && job.exitCode !== null ? ` (exit ${job.exitCode})` : ''}\nsession_id: ${sid}\n`;
  if (last) s += `\n--- final message ---\n${last}\n`;
  else s += `\n--- output so far ---\n${tail(out)}\n`;
  return s;
}

const tools = [
  { name: 'codex_start', description: 'Start a background Codex job (codex exec). Returns a job id immediately; poll with codex_status. Pass resume_session (session_id from a previous job) to continue that Codex conversation.',
    inputSchema: { type: 'object', required: ['prompt'], properties: {
      prompt: { type: 'string' },
      sandbox: { type: 'string', enum: ['read-only', 'workspace-write', 'danger-full-access'], description: 'Default read-only.' },
      model: { type: 'string' }, cwd: { type: 'string', description: 'Defaults to the WrappedWine repo.' },
      resume_session: { type: 'string' } } } },
  { name: 'codex_status', description: 'Get status and output of a Codex job.', inputSchema: { type: 'object', required: ['job_id'], properties: { job_id: { type: 'string' } } } },
  { name: 'codex_list', description: 'List Codex jobs.', inputSchema: { type: 'object', properties: {} } },
  { name: 'codex_cancel', description: 'Stop a running Codex job.', inputSchema: { type: 'object', required: ['job_id'], properties: { job_id: { type: 'string' } } } },
];

readline.createInterface({ input: process.stdin }).on('line', (line) => {
  let msg; try { msg = JSON.parse(line); } catch { return; }
  const { id, method, params } = msg;
  if (id === undefined) return;
  try {
    if (method === 'initialize') return send({ jsonrpc: '2.0', id, result: { protocolVersion: params?.protocolVersion || '2024-11-05', capabilities: { tools: {} }, serverInfo: { name: 'codex-bridge', version: '1.0.0' } } });
    if (method === 'ping') return send({ jsonrpc: '2.0', id, result: {} });
    if (method === 'tools/list') return send({ jsonrpc: '2.0', id, result: { tools } });
    if (method === 'tools/call') {
      const a = params.arguments || {};
      let r;
      if (params.name === 'codex_start') { const j = startJob(a); r = text(`started job ${j.id}\nPoll with codex_status.`); }
      else if (params.name === 'codex_status') r = text(jobStatus(a.job_id));
      else if (params.name === 'codex_list') r = text(fs.readdirSync(JOBS).map(j => `${j}  ${jobs.get(j)?.status || 'past'}`).join('\n') || 'no jobs');
      else if (params.name === 'codex_cancel') { const j = jobs.get(a.job_id); if (j && j.status === 'running') { j.child.kill(); j.status = 'cancelled'; } r = text(j ? `job ${a.job_id}: ${j.status}` : 'no such running job'); }
      else r = { ...text('unknown tool'), isError: true };
      return send({ jsonrpc: '2.0', id, result: r });
    }
    send({ jsonrpc: '2.0', id, error: { code: -32601, message: 'Method not found' } });
  } catch (e) { send({ jsonrpc: '2.0', id, result: { ...text(String(e?.stack || e)), isError: true } }); }
});
