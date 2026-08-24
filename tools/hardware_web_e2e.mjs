import { readFile } from 'node:fs/promises';

const credentialPath = new URL('../.secrets/ui-test-credentials.json', import.meta.url);
const maximumRequestMs = 45_000;

function requireString(value, field) {
  if (typeof value !== 'string' || value.length === 0) {
    throw new Error(`Credential field '${field}' must be a non-empty string`);
  }
  return value;
}

function form(values) {
  return new URLSearchParams(values);
}

async function fetchWithin(url, options, timeoutMs) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try {
    return await fetch(url, {...options, signal: controller.signal});
  } finally {
    clearTimeout(timer);
  }
}

async function login(baseUrl, password) {
  const response = await fetchWithin(new URL('/login', baseUrl), {
    method: 'POST',
    body: form({password}),
    redirect: 'manual',
  }, maximumRequestMs);
  if (response.status !== 303) {
    throw new Error(`CardMind login failed with HTTP ${response.status}`);
  }
  const cookie = response.headers.get('set-cookie')?.match(/cm_session=[^;]+/)?.[0];
  if (cookie === undefined) {
    throw new Error('CardMind login response did not include a session cookie');
  }
  const session = await fetchWithin(new URL('/api/session', baseUrl), {
    headers: {Cookie: cookie},
  }, maximumRequestMs);
  if (!session.ok) {
    throw new Error(`CardMind session request failed with HTTP ${session.status}`);
  }
  const document = await session.json();
  return {cookie, csrf: requireString(document.csrf, 'csrf')};
}

async function request(baseUrl, auth, path, options) {
  const headers = new Headers(options.headers);
  headers.set('Cookie', auth.cookie);
  headers.set('X-CardMind-CSRF', auth.csrf);
  const response = await fetchWithin(new URL(path, baseUrl), {
    ...options,
    headers,
  }, maximumRequestMs);
  if (!response.ok) {
    let detail = await response.text();
    try {
      detail = JSON.parse(detail).error ?? detail;
    } catch {
      // Preserve a non-JSON error body for diagnostics.
    }
    throw new Error(`${path} failed with HTTP ${response.status}: ${detail}`);
  }
  return response;
}

async function state(baseUrl, auth) {
  const statusResponse = await request(
    baseUrl,
    auth,
    '/api/state?view=status',
    {method: 'GET'},
  );
  const status = await statusResponse.json();
  const sshResponse = await request(
    baseUrl,
    auth,
    '/api/state?view=ssh',
    {method: 'GET'},
  );
  return {...status, ...await sshResponse.json()};
}

async function timedStateView(baseUrl, auth, view) {
  const startedAt = performance.now();
  const response = await request(
    baseUrl,
    auth,
    `/api/state?view=${encodeURIComponent(view)}`,
    {method: 'GET'},
  );
  const body = await response.text();
  const elapsedMs = Math.round(performance.now() - startedAt);
  return {
    view,
    elapsed_ms: elapsedMs,
    response_bytes: Buffer.byteLength(body, 'utf8'),
    document: JSON.parse(body),
  };
}

function memorySnapshot(label, document) {
  return {
    label,
    free_heap: document.free_heap,
    largest_heap: document.largest_heap,
    minimum_heap: document.minimum_heap,
    ssh_open: document.ssh_terminal_open,
  };
}

function recordSnapshot(measurements, label, document) {
  const snapshot = memorySnapshot(label, document);
  measurements.push(snapshot);
  console.log(JSON.stringify({stage: label, ...snapshot}));
}

async function startSsh(baseUrl, auth) {
  const response = await request(baseUrl, auth, '/api/ssh/start', {method: 'POST'});
  const document = await response.json();
  if (document.trust_required === true) {
    const trustResponse = await request(baseUrl, auth, '/api/ssh/trust', {
      method: 'POST',
      body: form({fingerprint: requireString(document.fingerprint, 'fingerprint')}),
    });
    const trusted = await trustResponse.json();
    if (trusted.open !== true) {
      throw new Error('SSH trust completed without opening the terminal');
    }
    return;
  }
  if (document.open !== true) {
    throw new Error('SSH start completed without opening the terminal');
  }
}

async function stopSsh(baseUrl, auth) {
  await request(baseUrl, auth, '/api/ssh/stop', {
    method: 'POST',
    body: form({}),
  });
}

async function verifyInteractiveSsh(baseUrl, auth) {
  const marker = 'SSH-E2E-OK';
  const editedCommand = `printf SSH-E2E-BADX${'\u007f'.repeat(4)}OK\r`;
  await request(baseUrl, auth, '/api/ssh/input', {
    method: 'POST',
    body: form({data: editedCommand}),
  });
  const deadline = performance.now() + maximumRequestMs;
  let output = '';
  while (performance.now() < deadline) {
    const response = await request(baseUrl, auth, '/api/ssh/output', {method: 'GET'});
    const document = await response.json();
    if (typeof document.output_base64 === 'string') {
      output += Buffer.from(document.output_base64, 'base64').toString('utf8');
      if (output.includes(marker)) {
        return;
      }
    }
    if (document.open !== true) {
      throw new Error('SSH terminal closed before returning the interactive marker');
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
  }
  throw new Error(`SSH terminal did not return marker '${marker}'`);
}

function parseSse(text) {
  const events = [];
  for (const frame of text.replace(/\r\n?/g, '\n').split('\n\n')) {
    if (!frame.startsWith('data:')) continue;
    events.push(JSON.parse(frame.slice(5)));
  }
  return events;
}

async function prompt(baseUrl, auth, marker) {
  const startedAt = performance.now();
  const response = await request(baseUrl, auth, '/api/prompt', {
    method: 'POST',
    body: form({prompt: `Reply with exactly: ${marker}`}),
  });
  const body = await response.text();
  const events = parseSse(body);
  const error = events.find((event) => event.type === 'error');
  if (error !== undefined) {
    throw new Error(`Prompt stream returned an error: ${error.error}`);
  }
  if (!events.some((event) => event.type === 'done')) {
    throw new Error('Prompt stream ended without a done event');
  }
  const responseText = events.map((event) => event.delta ?? '').join('');
  if (!responseText.includes(marker)) {
    throw new Error(`Prompt response did not contain marker '${marker}'`);
  }
  return Math.round(performance.now() - startedAt);
}

async function main() {
  const raw = JSON.parse(await readFile(credentialPath, 'utf8'));
  const baseUrl = new URL(requireString(raw.web_ui?.url, 'web_ui.url'));
  const password = requireString(
    raw.web_ui?.installation_password,
    'web_ui.installation_password',
  );
  const auth = await login(baseUrl, password);
  const measurements = [];
  const stateTimings = [];
  for (const view of ['status', 'chat', 'files', 'ssh', 'settings']) {
    const timing = await timedStateView(baseUrl, auth, view);
    stateTimings.push({
      view: timing.view,
      elapsed_ms: timing.elapsed_ms,
      response_bytes: timing.response_bytes,
    });
  }
  const baseline = await state(baseUrl, auth);
  console.log(JSON.stringify({stage: 'state_views', samples: stateTimings}));
  recordSnapshot(measurements, 'baseline', baseline);
  let sshStarted = false;
  try {
    const sshStartedAt = performance.now();
    await startSsh(baseUrl, auth);
    const sshConnectMs = Math.round(performance.now() - sshStartedAt);
    sshStarted = true;
    recordSnapshot(measurements, 'ssh_open', await state(baseUrl, auth));
    await verifyInteractiveSsh(baseUrl, auth);
    const activeSshPromptMs = await prompt(baseUrl, auth, 'WEB-E2E-SSH-OK');
    await stopSsh(baseUrl, auth);
    sshStarted = false;
    recordSnapshot(measurements, 'ssh_closed', await state(baseUrl, auth));
    const closedSshPromptMs = await prompt(baseUrl, auth, 'WEB-E2E-CLOSED-OK');
    console.log(JSON.stringify({
      result: 'pass',
      ssh_connect_ms: sshConnectMs,
      interactive_ssh: 'pass',
      active_ssh_prompt_ms: activeSshPromptMs,
      closed_ssh_prompt_ms: closedSshPromptMs,
      measurements,
    }));
  } finally {
    if (sshStarted) {
      await stopSsh(baseUrl, auth).catch((error) => {
        console.error(`SSH cleanup failed: ${error.message}`);
      });
    }
  }
}

await main();
