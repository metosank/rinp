//@ts-check

import APP_VERSION from "../app_version.mjs";
import { Rinp控制器元素 } from "./Rinp控制器元素.mjs";
import { 创建元素 } from "./基类元素.mjs";

const 外框元素 = document.createElement('div');
外框元素.className = '外框元素';
document.body.appendChild(外框元素);

const r = 创建元素(Rinp控制器元素);
r.设置固定api端点(location.href);
r.addEventListener('操作已触发', (e) => {
    console.log('操作已触发', e.detail.操作);
    if(e.detail.操作 === 1) {
        document.body.innerHTML = '';
        location.replace('about:blank');
    }
});
外框元素.appendChild(r);

const 底部信息 = document.createElement('div');
底部信息.className = '底部信息';
底部信息.textContent = `rinp WebUI ${APP_VERSION}`;
外框元素.appendChild(底部信息);

window.addEventListener('beforeunload',()=>{});