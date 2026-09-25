//@ts-check

import MagicString from 'magic-string'
import { stripLiteral } from 'strip-literal'

/**
 * @returns {import("vite").Plugin}
 */
export function cssUrlFixPlugin() {
  // 匹配 __u2css(import.meta.url, 'xxx.css', ...) 调用
  // 使用 's' 让 . 匹配换行，'g' 全局匹配，'d' 捕获索引（可选）
  const u2cssCallRE = /\b__u2css\s*\(\s*import\.meta\.url\s*,\s*(?:['"`].*?['"`]\s*,?\s*)+\)/dgs

  return {
    name: 'fix:css-u2css',
    enforce: 'pre',
    apply: 'build', // 只在构建时生效

    transform: {
      filter: {
        code: /__u2css\s*\(\s*import\.meta\.url/s
      },
      async handler(code, id) {
        const cleanString = stripLiteral(code)
        let match
        let s // MagicString 实例（懒初始化）
        let importCount = 0
        const importsToAdd = [] // 存储生成的 import 语句
        let needsCt2cssImport = false // 标记是否需要修补 import 语句

        // ─────────────────────────────────────
        // 1. 处理 __u2css 调用
        // ─────────────────────────────────────
        u2cssCallRE.lastIndex = 0
        while ((match = u2cssCallRE.exec(cleanString))) {
          const fullMatch = match[0]
          const startIndex = match.index
          const endIndex = startIndex + fullMatch.length
          
          // 从原始 code 截取真实片段（cleanString 已去除字符串内容，需用原始 code 提取路径）
          const originalSnippet = code.slice(startIndex, endIndex)
          
          // 提取引号内的路径参数（跳过 import.meta.url）
          const pathRE = /(['"`])(.*?)\1/g
          const paths = []
          let pathMatch
          
          while ((pathMatch = pathRE.exec(originalSnippet))) {
            const [, quote, rawPath] = pathMatch
            // 只处理 .css 文件，跳过动态模板字符串
            if (rawPath.includes('.css') && !rawPath.includes('${')) {
              paths.push({ path: rawPath, quote })
            }
          }

          if (paths.length === 0) continue

          // 懒初始化 MagicString
          if (!s) s = new MagicString(code)

          // 为每个 CSS 路径生成 import 变量和语句
          const replacementVars = []
          for (const { path, quote } of paths) {
            const importVar = `__vite_fix_css_content_${importCount++}`
            // ✅ 使用 ?inline 强制内联 CSS 内容（支持 minify 等处理）
            importsToAdd.push(`import ${importVar} from ${quote}${path}?inline${quote};\n`)
            replacementVars.push(importVar)
          }

          needsCt2cssImport = true
          
          // 替换调用: __u2css(import.meta.url, 'a.css', 'b.css') 
          //        → __ct2css(__vite_fix_css_content_0, __vite_fix_css_content_1)
          s.overwrite(startIndex, endIndex, `__ct2css(${replacementVars.join(', ')})`)
        }

        // ─────────────────────────────────────
        // 2. 修补 import 语句：添加 __ct2css
        // ─────────────────────────────────────
        if (needsCt2cssImport && !code.includes('__ct2css')) {
          if (!s) s = new MagicString(code)
          
          // 匹配 import { ... } from '...' 且包含 __u2css
          const importRE = /\bimport\s+\{([^}]*?)\}\s+from\s+(['"`][^'"`]+['"`])/g
          let importMatch
          
          while ((importMatch = importRE.exec(code)) !== null) {
            const [full, specifiers, source] = importMatch
            // 仅当包含 __u2css 且不包含 __ct2css 时修补
            if (specifiers.includes('__u2css') && !specifiers.includes('__ct2css')) {
              // 在 __u2css 后插入 __ct2css，保留原有格式
              const newSpecifiers = specifiers.replace(/(__u2css)(\s*,?)/, '$1, __ct2css$2')
              const startPos = importMatch.index
              const endPos = startPos + full.length
              const newImport = `import {${newSpecifiers}} from ${source}`
              s.overwrite(startPos, endPos, newImport)
            }
          }
        }

        // ─────────────────────────────────────
        // 3. 在文件顶部添加生成的 CSS import 语句
        // ─────────────────────────────────────
        if (s && importsToAdd.length > 0) {
          s.prepend(importsToAdd.join(''))
        }

        // ─────────────────────────────────────
        // 返回结果
        // ─────────────────────────────────────
        if (s) {
          return {
            code: s.toString(),
            map: s.generateMap({ hires: true, source: id, includeContent: true }).toString()
          }
        }
        
        return null
      }
    }
  }
}