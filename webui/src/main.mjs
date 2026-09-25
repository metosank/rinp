//@ts-check

import { Rinp控制器元素 } from "./Rinp控制器元素.mjs";
import { 创建元素 } from "./基类元素.mjs";
const r = 创建元素(Rinp控制器元素);
r.设置固定api端点(location.href);
r.addEventListener('操作已触发', (e) => {
    console.log('操作已触发', e.detail.操作);
    if(e.detail.操作 === 1) {
        document.body.innerHTML = '';
        location.replace('about:blank');
    }
});
document.body.appendChild(r);

window.addEventListener('beforeunload',()=>{});