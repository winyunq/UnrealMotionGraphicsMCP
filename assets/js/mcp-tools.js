/** Render the audited MCP catalog from assets/data/mcp-tools.json. */
(() => {
  let catalog = null;

  const labels = {
    zh: {
      read: '只读', write: '写入', delete: '删除需确认', compat: '兼容层',
      compile: '编译 / 保存', session: '上下文 / 会话', tools: '个工具',
      noResults: '没有找到匹配的工具。', loadError: '工具清单加载失败，请刷新页面。'
    },
    en: {
      read: 'Read', write: 'Write', delete: 'Confirmed deletion', compat: 'Compatibility',
      compile: 'Compile / save', session: 'Context / session', tools: 'tools',
      noResults: 'No matching tools found.', loadError: 'Could not load the tool catalog. Please refresh.'
    }
  };

  function create(tag, className, text) {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined) node.textContent = text;
    return node;
  }

  function currentLang() {
    return I18n.getCurrent() === 'en' ? 'en' : 'zh';
  }

  function renderCatalog() {
    if (!catalog) return;
    const lang = currentLang();
    const copy = labels[lang];
    const host = document.getElementById('mcp-tool-catalog');
    const nav = document.getElementById('mcp-group-nav');
    if (!host || !nav) return;

    host.replaceChildren();
    nav.replaceChildren();

    catalog.groups.forEach((group, groupIndex) => {
      const navLink = create('a', 'catalog-nav-link');
      navLink.href = `#group-${group.id}`;
      navLink.append(create('span', 'catalog-nav-number', String(groupIndex + 1)));
      navLink.append(create('span', '', group.title[lang]));
      navLink.append(create('span', 'catalog-nav-count', String(group.tools.length)));
      nav.append(navLink);

      const section = create('section', 'tool-group reveal visible');
      section.id = `group-${group.id}`;
      const head = create('div', 'tool-group-head');
      const identity = create('div', 'tool-group-identity');
      identity.append(create('span', 'tool-group-icon', group.icon));
      const titles = create('div');
      titles.append(create('div', 'eyebrow', `${String(groupIndex + 1).padStart(2, '0')} / ${catalog.groups.length}`));
      titles.append(create('h2', '', group.title[lang]));
      identity.append(titles);
      head.append(identity);
      head.append(create('span', 'tool-count-badge', `${group.tools.length} ${copy.tools}`));
      section.append(head);
      section.append(create('p', 'tool-group-summary', group.summary[lang]));

      const list = create('div', 'tool-list');
      group.tools.forEach(tool => {
        const row = create('article', 'tool-row');
        row.dataset.search = `${tool.name} ${tool.zh} ${tool.en}`.toLowerCase();
        const nameWrap = create('div', 'tool-name-wrap');
        nameWrap.append(create('code', 'tool-name', tool.name));
        nameWrap.append(create('span', `tool-kind tool-kind-${tool.kind}`, copy[tool.kind] || tool.kind));
        row.append(nameWrap);
        row.append(create('p', 'tool-description', tool[lang]));
        list.append(row);
      });
      section.append(list);
      host.append(section);
    });

    applySearch();
  }

  function applySearch() {
    const input = document.getElementById('tool-search');
    const result = document.getElementById('tool-search-result');
    if (!input || !result || !catalog) return;
    const query = input.value.trim().toLowerCase();
    let visible = 0;
    document.querySelectorAll('.tool-row').forEach(row => {
      const match = !query || row.dataset.search.includes(query);
      row.hidden = !match;
      if (match) visible += 1;
    });
    document.querySelectorAll('.tool-group').forEach(group => {
      const hasVisible = Array.from(group.querySelectorAll('.tool-row')).some(row => !row.hidden);
      group.hidden = !hasVisible;
    });
    const lang = currentLang();
    result.textContent = visible === 0
      ? labels[lang].noResults
      : `${visible} / ${catalog.source.toolCount}`;
  }

  async function loadCatalog() {
    const host = document.getElementById('mcp-tool-catalog');
    if (!host) return;
    try {
      const response = await fetch(`${getBase()}assets/data/mcp-tools.json`);
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      catalog = await response.json();
      renderCatalog();
      document.getElementById('tool-search')?.addEventListener('input', applySearch);
    } catch (error) {
      console.error('MCP catalog load error', error);
      host.textContent = labels[currentLang()].loadError;
    }
  }

  document.addEventListener('DOMContentLoaded', loadCatalog);
  document.addEventListener('umgmcp:languagechange', () => renderCatalog());
})();
