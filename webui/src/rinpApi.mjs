//@ts-check

export const 所有操作 = [
	'发送',
	'紧急退出',
	'中止',
	'暂停','恢复',
	'随机延迟关','随机延迟开',
	'增倍延迟关','增倍延迟开',
	'隐藏托盘图标','显示托盘图标'
];

const 荷载 = {
	检查连接: new Uint8Array([]).buffer,
	读取剪切板: new Uint8Array([0xff]).buffer,
}

export class RinpApi {
	api端点=''

	constructor() {
		
	}
	
	/**
	 * @param {string} api端点
	 */
	设置api端点(api端点) {
		this.api端点 = api端点;
	}

	/**
	 * @param {string} 方法
	 * @param {ArrayBuffer} [请求体]
	 * @returns {Promise<Response>}
	 */
	async 请求(方法, 请求体) {
		if (this.api端点 === '') throw new Error('API 端点未设置');
		const 响应 = await fetch(this.api端点, { method: 方法, body: 请求体 });
		if (!响应.ok) throw new Error('HTTP ' + 响应.status);
		return 响应;
	}

	async 检查连接() {
		const response = await this.请求('HEAD');
		return response.status;
	}

	/**
	 * @param {number} 操作
	 * @returns {Promise<Response>}
	 */
	async 发送操作(操作) {
		if(操作 < 1 || 操作 >= 所有操作.length) throw new Error('无效的操作');
		const bodyu=new Uint8Array([0x80 | 操作]);
		return await this.请求('POST', bodyu.buffer);
	}

	async 读取剪切板() {
		const response = await this.请求('POST', 荷载.读取剪切板);
		return response.text();
	}

	/**
	 * @param {string} 内容
	 * @param {number} 延迟
	 */
	async 发送文本(内容, 延迟) {
		const 编码器 = new TextEncoder().encode(内容);
		const 请求体 = new Uint8Array(2 + 编码器.length);
		请求体[0] = (延迟 >> 8) & 0x7f;
		请求体[1] = 延迟 & 0xff;
		请求体.set(编码器, 2);
		return this.请求('POST', 请求体.buffer);
	}
}
