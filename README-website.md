# UMG MCP Website — Deployment Guide

This document explains the website structure and how to deploy it.

## File Structure

```
/
├── index.html              # Hero landing page
├── open-source.html        # Open source version page
├── commercial.html         # Commercial Fab version page
├── features.html           # Features & comparison table
├── architecture.html       # Technical architecture & SVG diagrams
├── workflow.html           # Workflow & process diagrams
├── roadmap.html            # Development roadmap timeline
├── getting-started.html    # Quick start guide (tabbed: OSS + Commercial)
├── contact.html            # Contact & support page
├── README-website.md       # This file
│
└── assets/
    ├── css/
    │   └── main.css        # Main stylesheet (dark tech theme)
    ├── js/
    │   └── main.js         # i18n, nav, animations, interactions
    └── i18n/
        ├── en.json         # English translations
        └── zh.json         # Chinese (Simplified) translations
```

## Technology Stack

- **Pure HTML/CSS/JS** — no build tools, no frameworks
- **Vanilla JS** — no React/Vue/jQuery
- **CSS Custom Properties** — theming via CSS variables
- **IntersectionObserver** — scroll-triggered animations
- **JSON i18n** — client-side language switching
- **Inline SVG** — architecture and flow diagrams

## Local Development

Simply open any HTML file in a browser. For best results (especially i18n fetch), serve with a local HTTP server:

```bash
# Python
python -m http.server 8080

# Node.js
npx serve .

# VS Code
# Install "Live Server" extension, right-click index.html → Open with Live Server
```

Then visit: http://localhost:8080

## GitHub Pages Deployment

1. Push the repository to GitHub
2. Go to **Settings → Pages**
3. Set source to **Deploy from a branch**
4. Select `main` branch, `/ (root)` folder
5. Click **Save**

The site will be available at: `https://<username>.github.io/<repo>/`

> All paths use relative URLs so the site works at any base path.

## i18n System

Language switching works via:

1. `data-i18n="key"` attributes on HTML elements
2. `assets/i18n/en.json` and `assets/i18n/zh.json` translation files
3. `I18n.setLang('zh')` / `I18n.setLang('en')` in JS

**Default language**: Detected from `navigator.language`. Falls back to Chinese (`zh`) if not English.

**Persistence**: Language preference is saved to `localStorage` under key `umgmcp_lang`.

**Adding translations**:
1. Add the key/value to both `en.json` and `zh.json`
2. Add `data-i18n="your.key"` to the HTML element

## Navigation & Footer

Navigation and footer are rendered by JavaScript (`renderNav()` and `renderFooter()` in `main.js`). Each page only needs:

```html
<nav id="main-nav"></nav>
<!-- ... page content ... -->
<footer id="main-footer"></footer>
<script src="assets/js/main.js" defer></script>
```

The active nav link is automatically determined from `window.location.pathname`.

## Animations

| Animation | Trigger | Implementation |
|-----------|---------|----------------|
| Hero typing | Page load | JS interval loop |
| Particle field | Page load | JS DOM generation |
| Card reveal | Scroll into view | IntersectionObserver + `.reveal` class |
| Counter | Scroll into view | IntersectionObserver + `data-counter` |
| SVG flow lines | Scroll into view | IntersectionObserver + CSS stroke-dashoffset |
| Glow card | Mouse move | CSS custom property `--mx`/`--my` |
| FAQ accordion | Click | CSS max-height transition |
| Tab switch | Click | CSS display + fadeIn animation |

## Customization

### Colors
Edit CSS variables in `assets/css/main.css`:
```css
:root {
  --primary:   #7c3aed;   /* purple */
  --secondary: #2563eb;   /* blue */
  --accent:    #06b6d4;   /* cyan */
  /* ... */
}
```

### Adding a new page
1. Create `new-page.html` at root
2. Include standard head/nav/footer:
```html
<nav id="main-nav"></nav>
<!-- content -->
<footer id="main-footer"></footer>
<script src="assets/js/main.js" defer></script>
```
3. Add to the `pages` array in `renderNav()` in `main.js`
4. Add translation keys to both JSON files

## Links

- **GitHub**: https://github.com/winyunq/UnrealMotionGraphicsMCP
- **Fab Marketplace**: https://www.fab.com/zh-cn/listings/f70dbcb0-11e4-46bf-b3f7-9f30ba2c9b71
