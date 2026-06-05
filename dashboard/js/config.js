/**
 * 安置点信息大屏 - 配置
 * 巴法云 MQTT WebSocket
 */
window.DASHBOARD_CONFIG = {
  siteName: '灾后临时安置点 · 一号营地',

  /** WebSocket 连接地址（Mqtt WSS） */
  mqttUrl: 'wss://bemfa.com:9504/wss',

  /** 私钥（巴法云仅用 key 鉴权，无需特定 clientId） */
  clientId: 'YOUR_BEMFA_CLIENT_ID',

  username: '',
  password: '',

  /** 订阅主题（ESP32 上报的 topic） */
  topic: 'telemetry',

  /** Web↔Qt 交互主题（一键求助等） */
  helpTopic: 'help',

  /** 本地缓存有效分钟（断网时展示旧数据） */
  cacheMinutes: 30,

  /** 各指标的仪表盘量程 & 预警/报警阈值 */
  thresholds: {
    temperature: { gaugeMin: -10, gaugeMax: 60, warnOrangeLow: 32, dangerHigh: 38 },
    humidity:    { gaugeMin: 0,   gaugeMax: 100, warnOrangeLow: 70, dangerHigh: 85 },
    pm2_5:       { gaugeMin: 0,   gaugeMax: 500, warnOrangeLow: 75, dangerHigh: 150 },
    co2:         { gaugeMin: 0,   gaugeMax: 500, warnOrangeLow: 100, dangerHigh: 200 },
    electricity_current: { gaugeMin: 0, gaugeMax: 1000, warnOrangeLow: 500, dangerHigh: 700 },
    water_flow:  { gaugeMin: 0,   gaugeMax: 20,  warnOrangeLow: 5,   dangerHigh: 10 },
  },
};
