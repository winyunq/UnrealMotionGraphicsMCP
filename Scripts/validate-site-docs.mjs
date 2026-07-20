import { execFileSync } from 'node:child_process';
import { existsSync, readFileSync, readdirSync } from 'node:fs';
import { resolve } from 'node:path';

const root = process.cwd();
const refIndex = process.argv.indexOf('--source-ref');
const sourceRef = refIndex >= 0 ? process.argv[refIndex + 1] : 'origin/main';

function fail(message) {
  throw new Error(message);
}

function readJson(relativePath) {
  return JSON.parse(readFileSync(resolve(root, relativePath), 'utf8').replace(/^\uFEFF/, ''));
}

function gitShow(relativePath) {
  try {
    return execFileSync('git', ['show', `${sourceRef}:${relativePath}`], {
      cwd: root,
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe']
    });
  } catch (error) {
    fail(`Unable to read ${relativePath} from ${sourceRef}: ${error.stderr || error.message}`);
  }
}

function sortedUnique(values) {
  return [...new Set(values)].sort((a, b) => a.localeCompare(b));
}

const release = readJson('assets/data/release.json');
const catalog = readJson('assets/data/mcp-tools.json');
const zh = readJson('assets/i18n/zh.json');
const en = readJson('assets/i18n/en.json');

const sourceServer = gitShow('Resources/Python/UmgMcpServer.py');
const sourcePlugin = JSON.parse(gitShow('UmgMcp.uplugin').replace(/^\uFEFF/, ''));
const sourceCommit = execFileSync('git', ['rev-parse', sourceRef], { cwd: root, encoding: 'utf8' }).trim();

const sourceTools = sortedUnique([
  ...[...sourceServer.matchAll(/@register_tool\("([^"]+)"/g)].map(match => match[1]),
  ...[...sourceServer.matchAll(/@mcp\.tool\(name="([^"]+)"/g)].map(match => match[1])
]);
const catalogTools = catalog.groups.flatMap(group => group.tools.map(tool => tool.name));
const uniqueCatalogTools = sortedUnique(catalogTools);

if (catalogTools.length !== uniqueCatalogTools.length) {
  fail(`Catalog contains duplicate tool names (${catalogTools.length} entries, ${uniqueCatalogTools.length} unique).`);
}
if (sourceTools.length !== release.toolCount || catalogTools.length !== release.toolCount) {
  fail(`Tool count drift: source=${sourceTools.length}, catalog=${catalogTools.length}, release=${release.toolCount}.`);
}
if (catalog.groups.length !== release.toolGroups) {
  fail(`Tool group drift: catalog=${catalog.groups.length}, release=${release.toolGroups}.`);
}

const missingFromCatalog = sourceTools.filter(name => !uniqueCatalogTools.includes(name));
const missingFromSource = uniqueCatalogTools.filter(name => !sourceTools.includes(name));
if (missingFromCatalog.length || missingFromSource.length) {
  fail(`Tool-name drift. Missing from catalog: ${missingFromCatalog.join(', ') || 'none'}. Missing from source: ${missingFromSource.join(', ') || 'none'}.`);
}

if (sourcePlugin.VersionName !== release.version || sourcePlugin.Version !== release.internalVersion) {
  fail(`Version drift: source=${sourcePlugin.VersionName}/${sourcePlugin.Version}, docs=${release.version}/${release.internalVersion}.`);
}
if (sourceCommit !== release.sourceCommit || catalog.source.commit !== release.sourceCommit) {
  fail(`Source commit drift: source=${sourceCommit}, release=${release.sourceCommit}, catalog=${catalog.source.commit}.`);
}

const htmlFiles = readdirSync(root).filter(name => name.endsWith('.html'));
const translationKeys = new Set();
for (const file of htmlFiles) {
  const html = readFileSync(resolve(root, file), 'utf8');
  for (const match of html.matchAll(/data-i18n="([^"]+)"/g)) translationKeys.add(match[1]);

  for (const match of html.matchAll(/(?:href|src)="([^"#?]+)(?:#[^"]*)?"/g)) {
    const target = match[1];
    if (/^(?:https?:|mailto:|data:)/i.test(target) || target.startsWith('/')) continue;
    if (!existsSync(resolve(root, target))) fail(`${file} links to missing local target: ${target}`);
  }
}

const mainJs = readFileSync(resolve(root, 'assets/js/main.js'), 'utf8');
for (const match of mainJs.matchAll(/key:\s*'([^']+)'/g)) translationKeys.add(match[1]);

const zhKeys = Object.keys(zh);
const enKeys = Object.keys(en);
const missingZh = [...translationKeys].filter(key => !zhKeys.includes(key));
const missingEn = [...translationKeys].filter(key => !enKeys.includes(key));
const onlyZh = zhKeys.filter(key => !enKeys.includes(key));
const onlyEn = enKeys.filter(key => !zhKeys.includes(key));
if (missingZh.length || missingEn.length || onlyZh.length || onlyEn.length) {
  fail(`Translation drift. missing zh=${missingZh.join(', ') || 'none'}; missing en=${missingEn.join(', ') || 'none'}; only zh=${onlyZh.join(', ') || 'none'}; only en=${onlyEn.join(', ') || 'none'}.`);
}

const staleText = [
  /40\+\s*(?:MCP|Tools?|工具)/i,
  /UE\s*5\.3\+/i,
  /No Python\.\s*No Setup\./i,
  /zero-setup commercial/i,
  /Style\s*&amp;\s*Theming API[\s\S]{0,180}Partial/i,
  /Complex Sequencer[\s\S]{0,180}Basic/i
];
const auditedFiles = [
  'index.html', 'mcp-tools.html', 'getting-started.html',
  'assets/i18n/zh.json', 'assets/i18n/en.json'
];
for (const file of auditedFiles) {
  const text = readFileSync(resolve(root, file), 'utf8');
  for (const pattern of staleText) {
    if (pattern.test(text)) fail(`${file} still contains stale claim matching ${pattern}.`);
  }
}

console.log(`Documentation validation passed: ${release.version}, ${release.toolCount} tools, ${release.toolGroups} groups, source ${sourceCommit.slice(0, 7)}.`);
