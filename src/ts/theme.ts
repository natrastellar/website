const DARK_THEME_CLASS = 'dark';
const LIGHT_THEME_CLASS = 'light';

const THEME_KEY = 'theme';
const DARK_THEME_VALUE = 'dark';
const LIGHT_THEME_VALUE = 'light';

document.addEventListener('DOMContentLoaded', () => {
  const themeToggle = document.getElementById('theme-toggle');
  themeToggle?.addEventListener('click', () => {
    toggleTheme();
  });
  if (localStorage.getItem(THEME_KEY) === DARK_THEME_VALUE) {
    setTheme(true);
  }
  else if (localStorage.getItem(THEME_KEY) === LIGHT_THEME_VALUE) {
    setTheme(false);
  }
});

function toggleTheme() {
  const theme = localStorage.getItem(THEME_KEY);
  const dark = theme ? theme === DARK_THEME_VALUE : window.matchMedia('(prefers-color-scheme: dark)').matches;
  setTheme(!dark);
}

function setTheme(dark: boolean) {
  let root = document.getRootNode().parentElement;
  if (root) {
    setElementTheme(root, dark);
  }
  setElementTheme(document.body, dark);
  const themedElementTags = ['h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'a', 'button', 'nav', 'label', 'svg', 'header', 'li', 'footer', 'code'];
  for (const name of themedElementTags) {
    const elements = document.getElementsByTagName(name);
    for (let i = 0; i < elements.length; ++i) {
      setElementTheme(elements.item(i)!, dark);
    }
  }
  const themedElementClasses = ['project-card', 'project-tag', 'post-meta', 'article-update', 'article-note', 'post-link-header', 'footer-main', 'table-of-contents'];
  for (const name of themedElementClasses) {
    const elements = document.getElementsByClassName(name);
    for (let i = 0; i < elements.length; ++i) {
      setElementTheme(elements.item(i)!, dark);
    }
  }
  localStorage.setItem(THEME_KEY, dark ? DARK_THEME_VALUE : LIGHT_THEME_VALUE);
}

function setElementTheme(element: Element, dark: boolean) {
  element.classList.toggle(DARK_THEME_CLASS, dark);
  element.classList.toggle(LIGHT_THEME_CLASS, !dark);
}