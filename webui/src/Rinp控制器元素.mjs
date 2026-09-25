//@ts-check
import {所有操作, RinpApi} from './rinpApi.mjs';
import { __u2css, 基类带影子元素, 定义元素 } from './基类元素.mjs';

/**
 * @extends {CustomEvent<{操作: number}>}
 */
export class 操作将触发 extends CustomEvent {}
/**
 * @extends {CustomEvent<{操作: number}>}
 */
export class 操作已触发 extends CustomEvent {}

export class Rinp控制器元素 extends 基类带影子元素 {

    static 标签名称='test-result-element';
    static css列表=__u2css(import.meta.url,'./片段.css','./Rinp控制器.css');

    constructor() {
        super();
        this.api = new RinpApi();

        this.输入延迟 = 200;

        const api端点容器 = document.createElement('div');
        api端点容器.className = 'api端点容器';
        this.外框元素.appendChild(api端点容器);
        this.api端点容器 = api端点容器;

            const api端点外label = document.createElement('label');
            api端点外label.textContent = 'API 端点';
            api端点外label.htmlFor = 'api端点输入框';
            api端点容器.appendChild(api端点外label);

            const api端点外div = document.createElement('div');
            api端点外div.className = '输入flex';
            api端点容器.appendChild(api端点外div);

                this.api端点输入框 = document.createElement('input');
                this.api端点输入框.id = 'api端点输入框';
                this.api端点输入框.type = 'url';
                this.api端点输入框.inputMode = 'url';
                this.api端点输入框.placeholder = '输入 API 端点';
                this.api端点输入框.addEventListener('change', ()=>this.api.设置api端点(this.api端点输入框.value.trim()));
                api端点外div.appendChild(this.api端点输入框);

                this.检查连接按钮 = document.createElement('button');
                this.检查连接按钮.textContent = '检查连接';
                this.检查连接按钮.addEventListener('click', ()=>this.过程禁用(()=>this.检查连接()));
                api端点外div.appendChild(this.检查连接按钮);


        const 输入文本label = document.createElement('label');
        输入文本label.textContent = '文本';
        输入文本label.htmlFor = '输入文本输入框';
        this.外框元素.appendChild(输入文本label);

        this.输入文本输入框 = document.createElement('textarea');
        this.输入文本输入框.placeholder = '要输入的文本';
        this.输入文本输入框.id = '输入文本输入框';
        this.外框元素.appendChild(this.输入文本输入框);
            

        const 输入文本flex = document.createElement('div');
        输入文本flex.className = '输入flex';
        this.外框元素.appendChild(输入文本flex);

            const 输入文本内 = document.createElement('span');
            输入文本flex.appendChild(输入文本内);

                this.延迟输入框 = document.createElement('input');
                this.延迟输入框.id = '延迟输入框';
                this.延迟输入框.type = 'number';
                this.延迟输入框.inputMode = 'numeric';
                this.延迟输入框.min = '0';
                this.延迟输入框.max = '32767';
                this.延迟输入框.step = '200';
                this.延迟输入框.maxLength = 5;
                this.延迟输入框.valueAsNumber = this.输入延迟;
                this.延迟输入框.addEventListener('input', (e)=>{
                    if(this.延迟输入框.value === '') return;
                    const val = this.延迟输入框.valueAsNumber;
                    if (isNaN(val)) this.延迟输入框.valueAsNumber = 200;
                    if(val < 0) this.延迟输入框.valueAsNumber = 0;
                    if(val > 32767) this.延迟输入框.valueAsNumber = 32767;
                    this.输入延迟 = this.延迟输入框.valueAsNumber;
                });
                输入文本内.appendChild(this.延迟输入框);

                const 延迟单位 = document.createElement('span');
                延迟单位.className = 'flex内文本';
                延迟单位.textContent = 'ms';
                输入文本内.appendChild(延迟单位);
            
            this.发送按钮 = document.createElement('button');
            this.发送按钮.className = '主要按钮';
            this.发送按钮.textContent = '发送';
            this.发送按钮.addEventListener('click', ()=>this.过程禁用(()=>this.发送文本()));
            输入文本flex.appendChild(this.发送按钮);

        const 所有操作按钮 = [];
        this.操作按钮容器 = document.createElement('div');
        this.外框元素.appendChild(this.操作按钮容器);
        this.操作按钮容器.className = '其他操作按钮容器';
        for (let i = 1; i < 所有操作.length; i++ ) {
            const b = document.createElement('button');
            b.textContent = 所有操作[i];
            b.addEventListener('click', ()=>this.过程禁用(()=>this.发送操作(i)));
            所有操作按钮.push(b);
            if(i===2){
                输入文本flex.appendChild(b);
            }else{
                this.操作按钮容器.appendChild(b);
            }
        }
        
        所有操作按钮[0].className = '危险按钮';

        this.所有操作按钮 = 所有操作按钮;


        const 剪切板label = document.createElement('label');
        剪切板label.textContent = '剪切板';
        this.外框元素.appendChild(剪切板label);

        const 剪切板操作容器 = document.createElement('div');
        剪切板操作容器.className = 'actions';
        this.外框元素.appendChild(剪切板操作容器);

            this.读取剪切板按钮 = document.createElement('button');
            this.读取剪切板按钮.type = 'button';
            this.读取剪切板按钮.textContent = '读取剪切板';
            this.读取剪切板按钮.addEventListener('click', ()=>this.过程禁用(()=>this.读取剪切板()));
            剪切板操作容器.appendChild(this.读取剪切板按钮);

            this.复制剪切板按钮 = document.createElement('button');
            this.复制剪切板按钮.type = 'button';
            this.复制剪切板按钮.textContent = '复制';
            this.复制剪切板按钮.className = 'secondary';
            this.复制剪切板按钮.addEventListener('click', ()=>this.复制到剪切板());
            剪切板操作容器.appendChild(this.复制剪切板按钮);

        this.剪切板内容 = document.createElement('textarea');
        this.剪切板内容.placeholder = '读取的剪切板内容';
        this.剪切板内容.readOnly = true;
        this.外框元素.appendChild(this.剪切板内容);
        
        this.状态 = document.createElement('div');
        this.状态.id = 'status';
        this.状态.role = 'status';
        this.状态.ariaLive = 'polite';
        this.外框元素.appendChild(this.状态);

        this.所有远程操作按钮 = [this.发送按钮, this.读取剪切板按钮, this.复制剪切板按钮, this.检查连接按钮, ...this.所有操作按钮];
        this.远程操作按钮已禁用 = false;
    }

    /**
     * @template {keyof HTMLElementEventMap} K
     * 
     * @overload
     * @param {'操作将触发'} type
     * @param {(ev: 操作将触发) => any} listener
     * @param {boolean | AddEventListenerOptions} [options]
     * @returns {void}
     * 
     * @overload
     * @param {'操作已触发'} type
     * @param {(ev: 操作已触发) => any} listener
     * @param {boolean | AddEventListenerOptions} [options]
     * @returns {void}
     * 
     * @overload
     * @param {K} type
     * @param {(this: HTMLElement, ev: HTMLElementEventMap[K]) => any} listener
     * @param {boolean | AddEventListenerOptions} [options]
     * @returns {void}
     * 
     * @overload
     * @param {string} type
     * @param {EventListenerOrEventListenerObject} listener
     * @param {boolean | AddEventListenerOptions} [options]
     * @returns {void}
     * 
     * @param {string} type
     * @param {any} listener
     * @param {boolean | AddEventListenerOptions} [options]
     */
    addEventListener(type,listener,options){
        super.addEventListener(type,listener,options);
    }

    /**
     * @param {string} api端点
     */
    设置固定api端点(api端点) {
        this.api.设置api端点(api端点);
        this.api端点容器.style.display = 'none';
    }

    /**
     * @param {boolean} 状态
     */
    设置禁用状态(状态) {
        this.远程操作按钮已禁用 = 状态;
        this.所有远程操作按钮.forEach(b => b.disabled = 状态);
    }

    /**
     * @param {function} 回调
     */
    async 过程禁用(回调) {
        if (this.远程操作按钮已禁用) throw new Error('已有操作正在进行');
        this.设置禁用状态(true);
        try {
            await 回调();
        } catch (error) {
            // @ts-ignore
            this.状态.textContent = '发生错误: ' + error.message;
            console.error('发生错误:', error);
        } finally {
            this.设置禁用状态(false);
        }
    }

    async 检查连接() {
        const status = await this.api.检查连接();
        if (status === 200) {
            this.状态.textContent = '连接成功';
        }else{
            this.状态.textContent = `连接失败，HTTP 状态码: ${status}`;
        }
    }

    async 发送文本() {
        const 文本 = this.输入文本输入框.value;
        await this.api.发送文本(文本, this.输入延迟);
        this.状态.textContent = '文本已发送';
    }

    /**
     * @param {number} 操作
     */
    async 发送操作(操作) {
        const e1 = new 操作将触发('操作将触发', { detail: { 操作 },cancelable:true });
        this.dispatchEvent(e1);
        if (e1.defaultPrevented) {
            this.状态.textContent = '操作已取消';
            return;
        }
        await this.api.发送操作(操作);
        this.状态.textContent = '已执行' + 所有操作[操作];
        const e2 = new 操作已触发('操作已触发', { detail: { 操作 } });
        this.dispatchEvent(e2);
    }

    async 读取剪切板() {
        const text = await this.api.读取剪切板();
        this.剪切板内容.value = text;
        this.状态.textContent = '已读取剪切板';
    }

    async 复制到剪切板() {
        try {
            if (navigator.clipboard && window.isSecureContext) {
                await navigator.clipboard.writeText(this.剪切板内容.value);
            } else {
                this.剪切板内容.focus();
                this.剪切板内容.select();
                if (!document.execCommand('copy')) throw new Error('浏览器不支持复制');
            }
            this.状态.textContent = '已复制';
        } catch (error) {
            // @ts-ignore
            this.状态.textContent = '复制失败: ' + error.message;
        }
    }
}


定义元素(Rinp控制器元素);