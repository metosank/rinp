# rinp webui

`index.html` 是内嵌页面的源文件，使用当前页面 URL 作为固定的 API 端点。`test.html` 是浏览器测试页面，保留了可编辑的端点字段。

首次构建需使用 `npm install` 安装依赖。运行`npm run build` 会将 Vite 输出构建到 `dist`；随后顶层的 Makefile 会压缩该输出并生成 `../build/generated/web_page.cpp`。
