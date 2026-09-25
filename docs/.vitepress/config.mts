import { defineConfig, type MarkdownLocaleOptions } from 'vitepress'


export const zhMarkdown: MarkdownLocaleOptions = {
  container: {
    tipLabel: '提示',
    infoLabel: '信息',
    warningLabel: '警告',
    dangerLabel: '危险',
    detailsLabel: '详细信息',
    noteLabel: '注意',
    importantLabel: '重要',
    cautionLabel: '小心'
  },
  codeCopyButton: {
    tooltipText: '复制代码',
    copiedText: '已复制'
  }
}

// https://vitepress.dev/reference/site-config
export default defineConfig({
  title: "rinp-docs",
  description: "rinp documents",
    head: [
      [
        'link',{ rel: 'icon', href: '/f.min.svg' }
      ]
    ],
  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: '主页', link: '/' },
      { text: '下载', link: 'readme.md#下载' }
    ],

    sidebar: [
      {
        text: '页面',
        items: [
          { text: '说明', link: '/readme.md' }
        ]
      }
    ],

    socialLinks: [
      { icon: 'github', link: 'https://github.com/metosank/rinp' }
    ],
  
    editLink: {
      pattern: 'https://github.com/metosank/rinp/edit/main/docs/:path',
      text: '在 GitHub 上编辑此页面'
    },

    docFooter: {
      prev: '上一页',
      next: '下一页'
    },

    outline: {
      label: '页面导航'
    },

    lastUpdated: {
      text: '最后更新于'
    },

    notFound: {
      title: '页面未找到',
      quote:
        '但如果你不改变方向，并且继续寻找，你可能最终会到达你所前往的地方。',
      linkLabel: '前往首页',
      linkText: '带我回首页'
    },

    langMenuLabel: '多语言',
    returnToTopLabel: '回到顶部',
    sidebarMenuLabel: '菜单',
    darkModeSwitchLabel: '主题',
    lightModeSwitchTitle: '切换到浅色模式',
    darkModeSwitchTitle: '切换到深色模式',
    skipToContentLabel: '跳转到内容'
  },
  locales: {
    root: { label: '简体中文', lang: 'zh-Hans', dir: 'ltr', markdown: zhMarkdown },
  },
})
