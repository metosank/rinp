//@ts-check

/**
 * @typedef {Record<string, any>} 属性定义
 */


/**
 * @template {属性定义} [P={}] 
 */
class 基类元素 extends HTMLElement{
    /**
     * @abstract
     * @type {string}
     */
    static 标签名称;

    constructor(){
        super();

        this.已连接=false;
    }

    connectedCallback(){
        this.已连接=true;
    }

    disconnectedCallback(){
        this.已连接=false;
    }

}



/**
 * @param {typeof 基类元素<属性定义>} 类
 */
const 定义元素=(类)=>{
    customElements.define(类.标签名称,类);
}


/**
 * @template {typeof 基类元素 | typeof 基类元素<属性定义>} T
 * @param {T} 类
 * @returns {InstanceType<T>}
 */
const 创建元素=(类)=>{
    // @ts-ignore
    return document.createElement(类.标签名称);
}


/**
 * @template {属性定义} [P={}]
 * @extends 基类元素<P>
 */
class 基类带影子元素 extends 基类元素{

    /**
     * @type {CSSStyleSheet[]}
     */
    static css列表=[];

    /**
     * @type {CSSStyleSheet[]|null}
     */
    static _原型链css列表=null;

    static 获取原型链css列表(){
        if(this._原型链css列表===null){
            this._原型链css列表=获取原型链css列表(this);
        }
        return this._原型链css列表;
    }

    constructor(){
        super();
        const shadow=this.attachShadow({ mode:'open'});
        shadow.adoptedStyleSheets=new.target.获取原型链css列表();

        const 外框元素=document.createElement('div');
        外框元素.classList.add('外框');

        this.shadow=shadow;
        this.外框元素=外框元素;

        shadow.appendChild(外框元素);
    }
}

/**
 * @param {typeof 基类带影子元素} 类
 */
const 获取原型链css列表=(类)=>{
    /**
     * @type {CSSStyleSheet[]}
     */
    let css列表=[];
    let 当前类=类;
    while(当前类 && 当前类!==HTMLElement && 当前类!==基类带影子元素){
        if(当前类.css列表){
            css列表=当前类.css列表.concat(css列表);
        }
        当前类=Object.getPrototypeOf(当前类);
    }
    return css列表;
}

/**
 * @param  {...string} cts
 */
const __ct2css=(...cts)=>{
    return cts.map(ct=>{
        const c=new CSSStyleSheet();
        c.replace(ct);
        return c;
    });
}

/**
 * @param {string} base
 * @param  {...string} urls 
 */
const __u2css=(base,...urls)=>{
    return urls.map(u=>{
        const c=new CSSStyleSheet({baseURL:base});
        fetch(new URL(u,base)).then(r=>r.text()).then(t=>c.replace(t));
        return c;
    });
}

export {
    基类元素, 基类带影子元素,
    定义元素, 创建元素,
    __u2css,__ct2css,
};