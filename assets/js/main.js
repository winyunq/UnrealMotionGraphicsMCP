/**
 * UMG MCP — Main JavaScript
 * Handles: i18n, navigation, animations, interactions
 */

/* ── i18n ────────────────────────────────────────────── */
const I18n = (() => {
  let cache = {};
  let currentLang = 'zh';

  function detectLang() {
    const saved = localStorage.getItem('umgmcp_lang');
    if (saved) return saved;
    const browser = (navigator.language || navigator.userLanguage || 'zh').toLowerCase();
    return browser.startsWith('zh') ? 'zh' : 'en';
  }

  async function load(lang) {
    if (cache[lang]) return cache[lang];
    const base = getBasePath();
    try {
      const res = await fetch(`${base}assets/i18n/${lang}.json`);
      if (!res.ok) throw new Error('Failed to load i18n');
      cache[lang] = await res.json();
      return cache[lang];
    } catch (e) {
      console.warn('i18n load error', e);
      return {};
    }
  }

  function getBasePath() {
    const scripts = document.querySelectorAll('script[src]');
    for (const s of scripts) {
      if (s.src.includes('assets/js/main.js')) {
        return s.src.replace('assets/js/main.js', '');
      }
    }
    // fallback: count path depth
    const depth = location.pathname.split('/').filter(Boolean).length;
    return depth > 1 ? '../'.repeat(depth - 1) : './';
  }

  function apply(strings) {
    document.querySelectorAll('[data-i18n]').forEach(el => {
      const key = el.getAttribute('data-i18n');
      if (strings[key] !== undefined) {
        if (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA') {
          el.placeholder = strings[key];
        } else {
          el.textContent = strings[key];
        }
      }
    });
    document.querySelectorAll('[data-i18n-html]').forEach(el => {
      const key = el.getAttribute('data-i18n-html');
      if (strings[key] !== undefined) el.innerHTML = strings[key];
    });
    document.querySelectorAll('[data-i18n-title]').forEach(el => {
      const key = el.getAttribute('data-i18n-title');
      if (strings[key] !== undefined) el.title = strings[key];
    });
  }

  async function setLang(lang, animate = true) {
    currentLang = lang;
    localStorage.setItem('umgmcp_lang', lang);
    const strings = await load(lang);
    if (animate) {
      document.body.style.opacity = '0.85';
      document.body.style.transition = 'opacity 0.15s';
    }
    apply(strings);
    if (animate) {
      setTimeout(() => { document.body.style.opacity = '1'; }, 150);
    }
    // update lang buttons
    document.querySelectorAll('.lang-btn').forEach(btn => {
      btn.classList.toggle('active', btn.dataset.lang === lang);
    });
    document.documentElement.lang = lang === 'zh' ? 'zh-CN' : 'en';
  }

  function getCurrent() { return currentLang; }
  function getString(key) { return cache[currentLang]?.[key] || key; }

  return { detectLang, setLang, getCurrent, getString };
})();

/* ── Base path helper ──────────────────────────────────── */
function getBase() {
  const scripts = document.querySelectorAll('script[src]');
  for (const s of scripts) {
    if (s.src.includes('assets/js/main.js')) {
      return s.src.replace('assets/js/main.js', '');
    }
  }
  return './';
}

/* ── Navigation render ─────────────────────────────────── */
function renderNav() {
  const base = getBase();
  const pages = [
    { key: 'nav.home',         href: `${base}index.html`,         id: 'home' },
    { key: 'nav.features',     href: `${base}features.html`,      id: 'features' },
    { key: 'nav.architecture', href: `${base}architecture.html`,  id: 'architecture' },
    { key: 'nav.workflow',     href: `${base}workflow.html`,       id: 'workflow' },
    { key: 'nav.roadmap',      href: `${base}roadmap.html`,       id: 'roadmap' },
    { key: 'nav.docs',         href: `${base}getting-started.html`, id: 'docs' },
    { key: 'nav.commercial',   href: `${base}commercial.html`,    id: 'commercial' },
  ];

  const currentPage = location.pathname.split('/').pop().replace('.html','') || 'index';

  const linksHtml = pages.map(p => {
    const active = (currentPage === p.id || (p.id === 'home' && (currentPage === '' || currentPage === 'index'))) ? ' class="active"' : '';
    return `<a href="${p.href}"${active} data-i18n="${p.key}">${p.key}</a>`;
  }).join('');

  const mobileLinksHtml = pages.map(p =>
    `<a href="${p.href}" data-i18n="${p.key}">${p.key}</a>`
  ).join('');

  const navEl = document.getElementById('main-nav');
  if (!navEl) return;
  navEl.innerHTML = `
    <div class="container nav-inner">
      <a href="${base}index.html" class="nav-logo">
        <svg viewBox="0 0 28 28" fill="none" xmlns="http://www.w3.org/2000/svg">
          <rect width="28" height="28" rx="7" fill="url(#ng)"/>
          <path d="M7 14 L11 8 L17 18 L21 12" stroke="#fff" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/>
          <circle cx="21" cy="12" r="2" fill="#06b6d4"/>
          <defs>
            <linearGradient id="ng" x1="0" y1="0" x2="28" y2="28">
              <stop offset="0%" stop-color="#7c3aed"/>
              <stop offset="100%" stop-color="#2563eb"/>
            </linearGradient>
          </defs>
        </svg>
        <span>UMG MCP</span>
      </a>
      <nav class="nav-links" aria-label="Main navigation">
        ${linksHtml}
      </nav>
      <div class="nav-actions">
        <div class="lang-switcher" role="group" aria-label="Language switcher">
          <button class="lang-btn" data-lang="en">EN</button>
          <button class="lang-btn" data-lang="zh">中文</button>
        </div>
        <a href="https://www.fab.com/zh-cn/listings/f70dbcb0-11e4-46bf-b3f7-9f30ba2c9b71" target="_blank" rel="noopener" class="btn btn-primary btn-sm nav-cta" data-i18n="nav.cta">Buy on Fab →</a>
      </div>
      <button class="nav-toggle" id="nav-toggle" aria-label="Open menu" aria-expanded="false">
        <span></span><span></span><span></span>
      </button>
    </div>
    <nav class="nav-mobile" id="nav-mobile" aria-label="Mobile navigation">
      ${mobileLinksHtml}
      <div class="mobile-actions">
        <div class="lang-switcher">
          <button class="lang-btn" data-lang="en">EN</button>
          <button class="lang-btn" data-lang="zh">中文</button>
        </div>
        <a href="https://www.fab.com/zh-cn/listings/f70dbcb0-11e4-46bf-b3f7-9f30ba2c9b71" target="_blank" rel="noopener" class="btn btn-primary btn-sm" data-i18n="nav.cta">Buy on Fab →</a>
      </div>
    </nav>
  `;

  // Scroll behavior
  window.addEventListener('scroll', () => {
    navEl.classList.toggle('scrolled', window.scrollY > 20);
  }, { passive: true });

  // Mobile toggle
  const toggle = document.getElementById('nav-toggle');
  const mobileMenu = document.getElementById('nav-mobile');
  toggle?.addEventListener('click', () => {
    const open = mobileMenu.classList.toggle('open');
    toggle.setAttribute('aria-expanded', open);
    toggle.querySelectorAll('span').forEach((s, i) => {
      if (open) {
        if (i === 0) s.style.cssText = 'transform:rotate(45deg) translate(5px,5px)';
        if (i === 1) s.style.cssText = 'opacity:0';
        if (i === 2) s.style.cssText = 'transform:rotate(-45deg) translate(5px,-5px)';
      } else {
        s.style.cssText = '';
      }
    });
  });
  // close on link click
  mobileMenu?.querySelectorAll('a').forEach(a => {
    a.addEventListener('click', () => {
      mobileMenu.classList.remove('open');
      toggle.setAttribute('aria-expanded', 'false');
      toggle.querySelectorAll('span').forEach(s => { s.style.cssText = ''; });
    });
  });

  // Lang buttons
  document.querySelectorAll('.lang-btn').forEach(btn => {
    btn.addEventListener('click', () => I18n.setLang(btn.dataset.lang));
  });
}

/* ── Footer render ─────────────────────────────────────── */
function renderFooter() {
  const base = getBase();
  const footerEl = document.getElementById('main-footer');
  if (!footerEl) return;
  footerEl.innerHTML = `
    <div class="container">
      <div class="footer-grid">
        <div class="footer-brand">
          <a href="${base}index.html" class="nav-logo" style="text-decoration:none">
            <svg viewBox="0 0 28 28" fill="none" width="28" height="28">
              <rect width="28" height="28" rx="7" fill="url(#fg)"/>
              <path d="M7 14 L11 8 L17 18 L21 12" stroke="#fff" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round"/>
              <circle cx="21" cy="12" r="2" fill="#06b6d4"/>
              <defs>
                <linearGradient id="fg" x1="0" y1="0" x2="28" y2="28">
                  <stop offset="0%" stop-color="#7c3aed"/>
                  <stop offset="100%" stop-color="#2563eb"/>
                </linearGradient>
              </defs>
            </svg>
            <span style="background:linear-gradient(135deg,#7c3aed,#2563eb);-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text;font-weight:700">UMG MCP</span>
          </a>
          <p data-i18n="footer.brand.desc">The first MCP plugin fully focused on UE5 UMG.</p>
        </div>
        <div class="footer-col">
          <h4 data-i18n="footer.product">Product</h4>
          <ul>
            <li><a href="${base}features.html" data-i18n="footer.product.features">Features</a></li>
            <li><a href="${base}commercial.html" data-i18n="footer.product.commercial">Commercial Version</a></li>
            <li><a href="${base}roadmap.html" data-i18n="footer.product.roadmap">Roadmap</a></li>
          </ul>
        </div>
        <div class="footer-col">
          <h4 data-i18n="footer.resources">Resources</h4>
          <ul>
            <li><a href="${base}getting-started.html" data-i18n="footer.resources.docs">Get Started</a></li>
            <li><a href="${base}architecture.html" data-i18n="footer.resources.architecture">Architecture</a></li>
            <li><a href="${base}workflow.html" data-i18n="footer.resources.workflow">Workflow</a></li>
            <li><a href="https://github.com/winyunq/UnrealMotionGraphicsMCP" target="_blank" rel="noopener" data-i18n="footer.resources.github">GitHub</a></li>
          </ul>
        </div>
        <div class="footer-col">
          <h4 data-i18n="footer.community">Community</h4>
          <ul>
            <li><a href="https://github.com/winyunq/UnrealMotionGraphicsMCP/issues" target="_blank" rel="noopener" data-i18n="footer.community.issues">Report Issue</a></li>
            <li><a href="https://www.fab.com/zh-cn/listings/f70dbcb0-11e4-46bf-b3f7-9f30ba2c9b71" target="_blank" rel="noopener" data-i18n="footer.community.fab">Fab Marketplace</a></li>
            <li><a href="https://github.com/winyunq/UnrealMotionGraphicsMCP/discussions" target="_blank" rel="noopener" data-i18n="footer.community.discussions">Discussions</a></li>
          </ul>
        </div>
      </div>
      <div class="footer-bottom">
        <span data-i18n="footer.bottom.copy">© 2025 UMG MCP. Open source under MIT License.</span>
        <div style="display:flex;gap:16px">
          <a href="https://github.com/winyunq/UnrealMotionGraphicsMCP" target="_blank" rel="noopener" data-i18n="footer.bottom.oss">Open Source</a>
          <a href="https://www.fab.com/zh-cn/listings/f70dbcb0-11e4-46bf-b3f7-9f30ba2c9b71" target="_blank" rel="noopener" data-i18n="footer.bottom.fab">Fab Version</a>
        </div>
      </div>
    </div>
  `;
}

/* ── Scroll-reveal (IntersectionObserver) ──────────────── */
function initReveal() {
  const io = new IntersectionObserver((entries) => {
    entries.forEach(e => {
      if (e.isIntersecting) {
        e.target.classList.add('visible');
        io.unobserve(e.target);
      }
    });
  }, { threshold: 0.12, rootMargin: '0px 0px -40px 0px' });

  document.querySelectorAll('.reveal').forEach(el => io.observe(el));
}

/* ── Animated counters ─────────────────────────────────── */
function initCounters() {
  const io = new IntersectionObserver((entries) => {
    entries.forEach(e => {
      if (e.isIntersecting) {
        animateCounter(e.target);
        io.unobserve(e.target);
      }
    });
  }, { threshold: 0.5 });

  document.querySelectorAll('[data-counter]').forEach(el => io.observe(el));
}

function animateCounter(el) {
  const target = parseFloat(el.dataset.counter);
  const suffix = el.dataset.suffix || '';
  const prefix = el.dataset.prefix || '';
  const duration = 1800;
  const start = performance.now();
  function update(now) {
    const t = Math.min((now - start) / duration, 1);
    const ease = 1 - Math.pow(1 - t, 3);
    const val = Math.round(ease * target);
    el.textContent = prefix + val + suffix;
    if (t < 1) requestAnimationFrame(update);
  }
  requestAnimationFrame(update);
}

/* ── SVG flow animation ────────────────────────────────── */
function initSvgFlows() {
  const io = new IntersectionObserver((entries) => {
    entries.forEach(e => {
      if (e.isIntersecting) {
        e.target.querySelectorAll('.svg-flow-line, .svg-flow-arrow').forEach(el => {
          el.classList.add('animated');
        });
        io.unobserve(e.target);
      }
    });
  }, { threshold: 0.3 });

  document.querySelectorAll('.diagram-wrap, .arch-diagram').forEach(el => io.observe(el));
}

/* ── Glow card mouse tracking ──────────────────────────── */
function initGlowCards() {
  document.querySelectorAll('.glow-card').forEach(card => {
    card.addEventListener('mousemove', e => {
      const rect = card.getBoundingClientRect();
      const x = ((e.clientX - rect.left) / rect.width) * 100;
      const y = ((e.clientY - rect.top) / rect.height) * 100;
      card.style.setProperty('--mx', x + '%');
      card.style.setProperty('--my', y + '%');
    });
  });
}

/* ── Hero typing animation ─────────────────────────────── */
function initTyping() {
  const el = document.getElementById('typing-target');
  if (!el) return;

  const lang = I18n.getCurrent();
  const phrases = lang === 'zh'
    ? ['操作 UMG', '驱动 UI', '理解蓝图', '生成材质', '制作动画']
    : ['Speaks UMG', 'Builds UI', 'Edits Blueprints', 'Creates Materials', 'Drives Animations'];

  let phraseIdx = 0;
  let charIdx = 0;
  let deleting = false;
  let paused = false;

  el.style.borderRight = '3px solid var(--accent)';
  el.style.animation = 'blinkCaret 0.8s step-end infinite';

  function tick() {
    const phrase = phrases[phraseIdx];
    if (paused) {
      paused = false;
      deleting = true;
      setTimeout(tick, 1200);
      return;
    }
    if (!deleting) {
      el.textContent = phrase.slice(0, ++charIdx);
      if (charIdx === phrase.length) { paused = true; setTimeout(tick, 80); return; }
      setTimeout(tick, 80);
    } else {
      el.textContent = phrase.slice(0, --charIdx);
      if (charIdx === 0) {
        deleting = false;
        phraseIdx = (phraseIdx + 1) % phrases.length;
        setTimeout(tick, 400);
        return;
      }
      setTimeout(tick, 40);
    }
  }
  tick();
}

/* ── Hero particles ────────────────────────────────────── */
function initParticles() {
  const container = document.getElementById('hero-particles');
  if (!container) return;
  const colors = ['#7c3aed','#2563eb','#06b6d4','#9d5cf6'];
  const count = window.innerWidth < 768 ? 15 : 30;
  for (let i = 0; i < count; i++) {
    const p = document.createElement('div');
    p.className = 'particle';
    const size = 2 + Math.random() * 4;
    p.style.cssText = `
      width:${size}px;height:${size}px;
      background:${colors[Math.floor(Math.random()*colors.length)]};
      left:${Math.random()*100}%;
      animation-duration:${8+Math.random()*12}s;
      animation-delay:-${Math.random()*20}s;
    `;
    container.appendChild(p);
  }
}

/* ── FAQ accordion ─────────────────────────────────────── */
function initFAQ() {
  document.querySelectorAll('.faq-question').forEach(btn => {
    btn.addEventListener('click', () => {
      const answer = btn.nextElementSibling;
      const isOpen = btn.classList.contains('open');
      // close all
      document.querySelectorAll('.faq-question.open').forEach(b => {
        b.classList.remove('open');
        b.nextElementSibling?.classList.remove('open');
      });
      if (!isOpen) {
        btn.classList.add('open');
        answer?.classList.add('open');
      }
    });
  });
}

/* ── Tabs ──────────────────────────────────────────────── */
function initTabs() {
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      const group = btn.closest('[data-tab-group]') || btn.closest('.tabs-wrap');
      const target = btn.dataset.tab;
      group?.querySelectorAll('.tab-btn').forEach(b => b.classList.toggle('active', b === btn));
      group?.querySelectorAll('.tab-pane').forEach(p => p.classList.toggle('active', p.id === target));
    });
  });
}

/* ── Smooth anchor scroll ──────────────────────────────── */
function initSmoothScroll() {
  document.querySelectorAll('a[href^="#"]').forEach(a => {
    a.addEventListener('click', e => {
      const target = document.querySelector(a.getAttribute('href'));
      if (target) {
        e.preventDefault();
        const offset = 80;
        window.scrollTo({ top: target.getBoundingClientRect().top + window.scrollY - offset, behavior: 'smooth' });
      }
    });
  });
}

/* ── Main init ─────────────────────────────────────────── */
document.addEventListener('DOMContentLoaded', async () => {
  renderNav();
  renderFooter();

  const lang = I18n.detectLang();
  await I18n.setLang(lang, false);

  initParticles();
  initTyping();
  initReveal();
  initCounters();
  initSvgFlows();
  initGlowCards();
  initFAQ();
  initTabs();
  initSmoothScroll();
});
