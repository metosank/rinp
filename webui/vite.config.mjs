import { defineConfig } from 'vite';

import { createHtmlPlugin } from 'vite-plugin-html';
import { viteSingleFile } from 'vite-plugin-singlefile';
import { cssUrlFixPlugin } from './vite-plugin-fix-css-assets.mjs';

export default defineConfig({
	plugins: [
		cssUrlFixPlugin(),
		viteSingleFile(),
		createHtmlPlugin({
        	minify: true,
    	}),
	],
	build: {
		cssCodeSplit: false,
		assetsInlineLimit: 100000000,
		minify: 'terser',
		terserOptions: {
			compress: {
				// 危险操作：仅当确定没有动态属性访问、没有依赖外部API字段时才开启
				properties: {
					properties: true,
				},
			},
			mangle: {
				properties: {
					regex: /.*/,
				}
			}
		}
	}
});