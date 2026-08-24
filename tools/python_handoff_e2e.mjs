import {readFile} from 'node:fs/promises';

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

async function responseExcerpt(response, maximumLength) {
  const body = await response.text();
  return body.slice(0, maximumLength).replace(/[\r\n]+/g, ' ');
}

async function waitForResponse(url, options, acceptedStatuses, timeoutMs) {
  const deadline = performance.now() + timeoutMs;
  let lastError = new Error(`No response from ${url.origin}`);
  while (performance.now() < deadline) {
    try {
      const response = await fetchWithin(url, options, 3000);
      if (acceptedStatuses.includes(response.status)) {
        return response;
      }
      lastError = new Error(`${url.pathname} returned HTTP ${response.status}`);
    } catch (error) {
      lastError = error;
    }
    await new Promise((resolve) => setTimeout(resolve, 500));
  }
  throw new Error(`Timed out waiting for ${url.origin}: ${lastError.message}`);
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

async function pythonRequest(baseUrl, cookie, path, options) {
  const headers = new Headers(options.headers);
  headers.set('Cookie', cookie);
  const response = await fetchWithin(new URL(path, baseUrl), {
    ...options,
    headers,
  }, maximumRequestMs);
  if (!response.ok) {
    const detail = await responseExcerpt(response, 512);
    throw new Error(`${path} failed with HTTP ${response.status}: ${detail}`);
  }
  return response;
}

async function waitForScript(baseUrl, cookie, marker) {
  const deadline = performance.now() + maximumRequestMs;
  while (performance.now() < deadline) {
    const response = await pythonRequest(baseUrl, cookie, '/api/state', {method: 'GET'});
    const document = await response.json();
    if (document.running === false && document.output.includes(marker)) {
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 250));
  }
  throw new Error(`Python script did not finish with marker '${marker}'`);
}

async function deleteProbeFile(baseUrl, password, name) {
  const auth = await login(baseUrl, password);
  const response = await fetchWithin(new URL('/api/file/delete', baseUrl), {
    method: 'POST',
    headers: {
      Cookie: auth.cookie,
      'X-CardMind-CSRF': auth.csrf,
    },
    body: form({name}),
  }, maximumRequestMs);
  if (!response.ok) {
    const detail = await responseExcerpt(response, 512);
    throw new Error(`Probe cleanup failed with HTTP ${response.status}: ${detail}`);
  }
}

async function main() {
  const credentials = JSON.parse(await readFile(credentialPath, 'utf8'));
  const baseUrl = new URL(requireString(credentials.web_ui?.url, 'web_ui.url'));
  const password = requireString(
    credentials.web_ui?.installation_password,
    'web_ui.installation_password',
  );
  const auth = await login(baseUrl, password);
  const start = await fetchWithin(new URL('/api/python/start', baseUrl), {
    method: 'POST',
    headers: {
      Cookie: auth.cookie,
      'X-CardMind-CSRF': auth.csrf,
    },
    body: form({}),
  }, maximumRequestMs);
  if (!start.ok) {
    const detail = await responseExcerpt(start, 512);
    throw new Error(`Python start failed with HTTP ${start.status}: ${detail}`);
  }
  const transition = await start.json();
  const pythonUrl = new URL(requireString(transition.address, 'address'));
  const token = requireString(transition.handoff_token, 'handoff_token');
  const handoffUrl = new URL('/handoff', pythonUrl);
  handoffUrl.searchParams.set('token', token);
  const handoff = await waitForResponse(
    handoffUrl,
    {redirect: 'manual'},
    [303],
    maximumRequestMs,
  );
  const pythonCookie = handoff.headers.get('set-cookie')?.match(/cardmind_py=[^;]+/)?.[0];
  if (pythonCookie === undefined) {
    throw new Error('Python handoff did not issue an authenticated session cookie');
  }
  const page = await fetchWithin(pythonUrl, {
    headers: {Cookie: pythonCookie},
  }, maximumRequestMs);
  const html = await page.text();
  for (const fragment of [
    'id=sideSplitter',
    'id=outputSplitter',
    'output.scrollTop=output.scrollHeight',
    'Return to CardMind',
  ]) {
    if (!html.includes(fragment)) {
      throw new Error(`Python workspace is missing ${fragment}`);
    }
  }
  const state = await fetchWithin(new URL('/api/state', pythonUrl), {
    headers: {Cookie: pythonCookie},
  }, maximumRequestMs);
  if (!state.ok || !Array.isArray((await state.json()).files)) {
    throw new Error('Python workspace state endpoint is unavailable');
  }
  const probeName = `cardmind_e2e_${Date.now()}.py`;
  const probeMarker = `PYTHON-E2E-OK-${Date.now()}`;
  await pythonRequest(
    pythonUrl,
    pythonCookie,
    `/api/file?name=${encodeURIComponent(probeName)}`,
    {
      method: 'POST',
      headers: {'Content-Type': 'text/plain;charset=utf-8'},
      body: `print(${JSON.stringify(probeMarker)})\n`,
    },
  );
  await pythonRequest(
    pythonUrl,
    pythonCookie,
    `/api/run?name=${encodeURIComponent(probeName)}`,
    {method: 'POST'},
  );
  await waitForScript(pythonUrl, pythonCookie, probeMarker);
  let returnTransport = 'http_202';
  try {
    const returnResponse = await fetchWithin(new URL('/api/cardmind', pythonUrl), {
      method: 'POST',
      headers: {Cookie: pythonCookie},
    }, maximumRequestMs);
    if (returnResponse.status !== 202) {
      throw new Error(`Return to CardMind failed with HTTP ${returnResponse.status}`);
    }
  } catch (error) {
    if (error?.cause?.code !== 'UND_ERR_SOCKET') {
      throw error;
    }
    returnTransport = 'connection_closed_during_switch';
  }
  const ready = await waitForResponse(
    new URL('/api/session', baseUrl),
    {cache: 'no-store'},
    [200, 401],
    maximumRequestMs,
  );
  await deleteProbeFile(baseUrl, password, probeName);
  console.log(JSON.stringify({
    result: 'pass',
    python_page_bytes: Buffer.byteLength(html, 'utf8'),
    python_script: 'pass',
    return_transport: returnTransport,
    cardmind_session_status: ready.status,
  }));
}

await main();
