//@ts-check

/**@type {import("svgo").Config} */
export default {
  plugins: [
    {
      name: 'preset-default',
      params: {
        overrides: {
          // ⚠️ 关键：保留 viewBox，否则响应式缩放会失效
          removeViewBox: false,
          // 保留标题和描述（无障碍需要）
          removeTitle: false,
          removeDesc: false,
          // 浮点精度，3位通常足够，可进一步减小体积
          //cleanupNumericValues: { floatPrecision: 3 },
        },
      },
    },
    // 移除 XML 声明（前端内联时不需要）
    'removeXMLProcInst',
    // 移除注释
    'removeComments',
    // 移除编辑器元数据
    'removeMetadata',
    // 移除空白符
    'collapseGroups',
  ],
};
