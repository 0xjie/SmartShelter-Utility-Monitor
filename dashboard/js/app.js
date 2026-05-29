(function () {
  'use strict';

  const CFG = window.DASHBOARD_CONFIG || {};

  /* ===== 传感器显示定义 ===== */
  const METRICS = [
    { id: 't',   key: 't', label: '温度',   unit: '℃',     icon: '🌡️' },
    { id: 'h',   key: 'h', label: '湿度',   unit: '%',      icon: '💧' },
    { id: 'pm',  key: 'pm',label: 'PM2.5',  unit: 'μg/m³', icon: '💨' },
    { id: 'aq',  key: 'aq',label: '空气质量',unit: 'ppm',   icon: '🍃' },
  ];

  const TIPS = [
    '请节约用水用电，共同维护安置点秩序。',
    '注意通风，保持室内空气流通。',
    '如有身体不适，请及时联系工作人员。',
    '请勿私拉电线，注意用电安全。',
    '未来 24 小时环境状况良好，适合户外活动。',
  ];

  const FORECAST_TIPS = [
    '未来 24 小时温湿变化整体平稳，请适时开窗通风。',
    '夜间气温略降，休息时注意保暖；白天体感舒适。',
    '午后局部时段偏热，户外活动注意补水与防晒。',
    '空气质量以优良为主，敏感人群仍建议佩戴口罩。',
    '降雨概率较低，晾晒与出行相对适宜。',
  ];

  // 告警码→中文消息（STM32只发数字码）
  const ALARM_MSG = {
    101:'温度偏高',102:'温度过高告警',103:'温度偏低',104:'温度过低告警',
    111:'湿度偏高',112:'湿度过高告警',113:'湿度偏低',114:'湿度过低告警',
    121:'PM2.5偏高',122:'PM2.5超标告警',
    131:'空气质量下降',132:'空气质量恶化告警',
    141:'电流偏高',142:'电流过载告警',
    151:'水流偏高',152:'水流异常告警',
    200:'温度+PM2.5组合告警(火灾风险)',201:'PM2.5+AQ组合告警(烟雾污染)',202:'水流+电流组合告警(管道异常)',
    901:'DHT11离线',902:'MQ135离线',903:'GP2Y10离线',904:'ACS712离线',905:'水流传感器离线'
  };
  // 动作标志位→中文
  const ACT_FLAG_BUZZER_ON = 0x01, ACT_FLAG_BUZZER_OFF = 0x02;
  const ACT_FLAG_FAN_ON = 0x04, ACT_FLAG_FAN_OFF = 0x08;
  const ACT_FLAG_SERVO_OPEN = 0x10, ACT_FLAG_SERVO_CLOSE = 0x20;
  function actionFlagsToText(flags) {
    var acts = [];
    if (flags & ACT_FLAG_BUZZER_ON)  acts.push('蜂鸣器已开启');
    if (flags & ACT_FLAG_BUZZER_OFF) acts.push('蜂鸣器已关闭');
    if (flags & ACT_FLAG_FAN_ON)     acts.push('风扇已开启');
    if (flags & ACT_FLAG_FAN_OFF)    acts.push('风扇已关闭');
    if (flags & ACT_FLAG_SERVO_OPEN) acts.push('窗户已打开');
    if (flags & ACT_FLAG_SERVO_CLOSE)acts.push('窗户已关闭');
    return acts;
  }
  let mqttClient = null;
  let tipIndex = 0;
  let forecastTipIndex = 0;
  let currentLinkLevel = 0;
  let forecastData = null;
  const FC_COL_WIDTH = 120;
  const FC_COL_GAP = 16;


  /* ===== 今日已用（STM32直接上传，不做端侧累加） ===== */

  /* ===== 级别色映射 ===== */
  function lvClass(lv) { return lv === 2 ? 'danger' : lv === 1 ? 'warn' : 'ok'; }
  function lvTag(lv)   { return lv === 2 ? '告警' : lv === 1 ? '预警' : '正常'; }

  /* ===== 顶部状态栏 ===== */
  function updateTopBar(linkLv) {
    var icon = document.getElementById('statusIcon');
    var text = document.getElementById('statusText');
    var status = document.getElementById('sysStatus');
    if (!icon || !text || !status) return;
    if (linkLv === 0) {
      icon.textContent = '✓'; text.textContent = '系统运行正常';
      status.style.borderColor = 'rgba(16,185,129,0.3)';
    } else if (linkLv === 1) {
      icon.textContent = '!'; text.textContent = '系统存在预警';
      status.style.borderColor = 'rgba(245,158,11,0.3)';
    } else {
      icon.textContent = '✗'; text.textContent = '系统存在异常';
      status.style.borderColor = 'rgba(239,68,68,0.4)';
    }
  }

  /* ===== 左栏：2×2 指标卡（从 lv.d 读取级别，不再自行计算） ===== */
  function updateMetrics(data) {
    var sen = data.sen || {};
    var d = (data.lv && data.lv.d) ? data.lv.d : [0,0,0,0,0,0];
    var dlMap = { t: d[0] || 0, h: d[1] || 0, pm: d[2] || 0, aq: d[3] || 0, cur: d[4] || 0, flw: d[5] || 0 };

    METRICS.forEach(function (m) {
      var val = sen[m.key];
      var lv = dlMap[m.id] || 0;
      var cls = lvClass(lv);

      var valEl = document.getElementById('mval' + (m.id === 't' ? 'Temp' : m.id === 'h' ? 'Hum' : m.id === 'pm' ? 'Pm' : 'Aq'));
      var tagEl = document.getElementById('mtag' + (m.id === 't' ? 'Temp' : m.id === 'h' ? 'Hum' : m.id === 'pm' ? 'Pm' : 'Aq'));
      var cardEl = document.getElementById('mcard' + (m.id === 't' ? 'Temp' : m.id === 'h' ? 'Hum' : m.id === 'pm' ? 'Pm' : 'Aq'));

      if (valEl) valEl.textContent = (val != null && !Number.isNaN(val)) ? Number(val).toFixed(m.id === 'aq' ? 0 : 1) : '--';
      if (tagEl) tagEl.textContent = lvTag(lv);
      if (cardEl) cardEl.className = 'mcard mcard--' + cls;
    });

    // 同步阈值显示（QT 下发后 STM32 上行 th 字段）
    if (data.th) {
      var th = data.th;
      var rTemp = document.getElementById('mrangeTemp');
      var rHum  = document.getElementById('mrangeHum');
      var rPm   = document.getElementById('mrangePm');
      var rAq   = document.getElementById('mrangeAq');
      if (rTemp) rTemp.textContent = '告警 <' + (th.tb || '?') + ' 或 >' + (th.ta || '?') + '℃';
      if (rHum)  rHum.textContent  = '告警 <' + (th.hb || '?') + ' 或 >' + (th.ha || '?') + '%';
      if (rPm)   rPm.textContent   = '预警≥' + Math.round((th.pa || 150) / 2) + ' 告警≥' + (th.pa || '?') + 'μg/m³';
      if (rAq)   rAq.textContent   = '预警≥' + Math.round((th.aa || 200) / 2) + ' 告警≥' + (th.aa || '?') + 'ppm';
    }

    currentLinkLevel = (data.lv && data.lv.link != null) ? data.lv.link : 0;
    updateTopBar(currentLinkLevel);
  }

  /* ===== 中栏：水电管理（读STM32直接上传的累计值） ===== */
  function updateResources(data) {
    var res = data.res || {};
    var sen = data.sen || {};
    var bp = res.bp;
    var wp = res.wp;
    var amp = sen.i;
    var flow = sen.f;

    setBat(bp, amp, res.tu || 0, res.br || 0, res.bt || 0, res.ps || 0);
    setTank(wp, flow, res.wu || 0, res.wr || 0, res.wt || 0);
  }

  function setBat(pct, amp, usedMAh, remainMAh, remainMin, powerStatus) {
    var pctEl = document.getElementById('batPct');
    var fillEl = document.getElementById('batFill');
    if (pctEl) {
      pctEl.textContent = (pct != null && !Number.isNaN(pct))
        ? pct + '% (' + ((remainMAh || 0) / 1000).toFixed(1) + ' Ah)' : '--%';
    }
    if (fillEl) {
      var w = (pct != null && !Number.isNaN(pct)) ? Math.max(0, Math.min(pct, 100)) : 0;
      fillEl.setAttribute('y', 169 - 138 * w / 100);
      fillEl.setAttribute('height', 138 * w / 100);
      fillEl.setAttribute('fill', w >= 60 ? 'url(#batFillGrad)' : w >= 30 ? '#F59E0B' : '#EF4444');
    }
    var predEl = document.getElementById('powerPredict');
    var todayEl = document.getElementById('powerToday');
    var lineEl = document.getElementById('powerLineStatus');
    var badgeEl = document.getElementById('powerBadge');
    if (predEl && remainMin > 0) {
      if (remainMin >= 1440)
        predEl.innerHTML = '预计剩余可用时间：约 <strong>' + (remainMin / 1440).toFixed(1) + '</strong> 天';
      else if (remainMin >= 60)
        predEl.innerHTML = '预计剩余可用时间：约 <strong>' + Math.round(remainMin / 60) + '</strong> 小时';
      else
        predEl.innerHTML = '预计剩余可用时间：约 <strong>' + Math.round(remainMin) + '</strong> 分钟';
    } else if (predEl && amp <= 10) {
      predEl.innerHTML = '当前无负载';
    } else if (predEl) {
      predEl.innerHTML = '预计剩余可用时间：约 --';
    }
    if (todayEl) {
      todayEl.textContent = ((usedMAh || 0) / 1000).toFixed(1) + ' Ah';
    }
    if (lineEl) {
      if (powerStatus >= 2)      { lineEl.textContent = '过载'; lineEl.style.color = '#EF4444'; }
      else if (powerStatus >= 1) { lineEl.textContent = '偏高'; lineEl.style.color = '#F59E0B'; }
      else if ((amp || 0) > 10) { lineEl.textContent = '正常'; lineEl.style.color = ''; }
      else                       { lineEl.textContent = '--'; lineEl.style.color = ''; }
    }
    if (badgeEl) badgeEl.textContent = (pct != null && pct > 30) ? '电力供应充足' : (pct > 10 ? '电力不足' : '电力告急');
    var card = document.getElementById('resPower');
    if (card) card.classList.toggle('res-card--critical', pct != null && pct < 10);
  }

  function setTank(pct, flow, usedCL, remainCL, remainMin) {
    var pctEl = document.getElementById('wtrPct');
    var fillEl = document.getElementById('tankFill');
    var remainL = (remainCL || 0) / 100;  // cL→L，STM32直接值
    if (pctEl) {
      pctEl.textContent = (pct != null && !Number.isNaN(pct))
        ? pct + '% (' + remainL.toFixed(1) + ' L)' : '--%';
    }
    if (fillEl) {
      var w = (pct != null && !Number.isNaN(pct)) ? Math.max(0, Math.min(pct, 100)) : 0;
      fillEl.style.height = w + '%';
      fillEl.style.background = w >= 60
        ? 'linear-gradient(180deg, #60A5FA 0%, #3B82F6 50%, #2563EB 100%)'
        : w >= 30
          ? 'linear-gradient(180deg, #FBBF24 0%, #F59E0B 100%)'
          : 'linear-gradient(180deg, #F87171 0%, #EF4444 100%)';
    }
    var predEl = document.getElementById('waterPredict');
    var todayEl = document.getElementById('waterToday');
    var badgeEl = document.getElementById('waterBadge');
    if (predEl && remainMin > 0) {
      if (remainMin >= 1440)
        predEl.innerHTML = '预计剩余可用时间：约 <strong>' + (remainMin / 1440).toFixed(1) + '</strong> 天';
      else if (remainMin >= 60)
        predEl.innerHTML = '预计剩余可用时间：约 <strong>' + Math.round(remainMin / 60) + '</strong> 小时';
      else
        predEl.innerHTML = '预计剩余可用时间：约 <strong>' + Math.round(remainMin) + '</strong> 分钟';
    } else if (predEl && flow != null && flow <= 0.01) {
      predEl.innerHTML = '当前无用水';
    } else if (predEl) {
      predEl.innerHTML = '预计剩余可用时间：约 --';
    }
    if (todayEl) {
      todayEl.textContent = ((usedCL || 0) / 100).toFixed(1) + ' L';
    }
    if (badgeEl) badgeEl.textContent = (pct != null && pct > 30) ? '供水正常' : (pct > 10 ? '水量不足' : '缺水告急');
    var card = document.getElementById('resWater');
    if (card) card.classList.toggle('res-card--critical', pct != null && pct < 10);
  }

  // 通过告警码推断级别: 101,111,121,131,141,151 → WARN(1); 102,112,122,132,142,152,200-202 → ALARM(2)
  function alarmLvFromCode(code) { return (code % 10 === 2 || code >= 200) ? 2 : 1; }

  /* ===== 右栏：报警/预警（alm 为数字码数组，actn 为位掩码） ===== */
  function updateAlarms(data) {
    var codes = data.alm || [];
    var dangers = [], warns = [];
    (Array.isArray(codes) ? codes : []).forEach(function(c) {
      var lv = alarmLvFromCode(c);
      var msg = ALARM_MSG[c] || ('Code ' + c);
      (lv >= 2 ? dangers : warns).push({c: c, m: msg, lv: lv});
    });
    var actFlags = typeof data.actn === 'number' ? data.actn : 0;
    var actions = actionFlagsToText(actFlags);

    var alarmListEl  = document.getElementById('alarmList');
    var alarmStatusEl = document.getElementById('alarmStatus');
    var warnListEl   = document.getElementById('warnList');
    var warnStatusEl = document.getElementById('warnStatus');
    var now = new Date().toLocaleTimeString();

    if (alarmListEl && alarmStatusEl) {
      if (dangers.length === 0) {
        alarmListEl.innerHTML = '<span class="alarm-empty">✅ 当前无实时报警</span>';
        alarmStatusEl.textContent = '当前无实时报警'; alarmStatusEl.style.color = '#10B981';
      } else {
        alarmListEl.innerHTML = dangers.map(function(a) {
          return '<div class="alarm-item alarm-item--danger">' + now + ' ' + a.m + ' (码:' + a.c + ')</div>';
        }).join('') + (actions.length ? '<div class="alarm-action">已采取措施：' + actions.join(' / ') + '</div>' : '');
        alarmStatusEl.textContent = dangers.length + ' 条报警'; alarmStatusEl.style.color = '#EF4444';
      }
    }
    if (warnListEl && warnStatusEl) {
      if (warns.length === 0) {
        warnListEl.innerHTML = '<span class="alarm-empty">✅ 当前无预警信息</span>';
        warnStatusEl.textContent = '当前无预警信息'; warnStatusEl.style.color = '#10B981';
      } else {
        warnListEl.innerHTML = warns.map(function(a) {
          return '<div class="alarm-item alarm-item--warn">' + now + ' ' + a.m + ' (码:' + a.c + ')</div>';
        }).join('') + (actions.length ? '<div class="alarm-action">已采取措施：' + actions.join(' / ') + '</div>' : '');
        warnStatusEl.textContent = warns.length + ' 条预警'; warnStatusEl.style.color = '#F59E0B';
      }
    }
  }

  /* ===== Toast 弹窗（从 alm 读取） ===== */
  var lastToastLevel = 0;
  var toastTimer = null;

  function showLinkageToast(data) {
    var container = document.getElementById('linkageToast');
    if (!container) return;
    var linkLv = (data.lv && data.lv.link != null) ? data.lv.link : 0;
    if (linkLv === lastToastLevel) return;
    lastToastLevel = linkLv;

    if (toastTimer) clearTimeout(toastTimer);

    if (linkLv === 0) {
      container.className = 'linkage-toast';
      container.innerHTML = '';
      return;
    }

    var codes = (Array.isArray(data.alm) ? data.alm : []).map(function(c) { return typeof c === 'number' ? c : (c.c || 0); });
    var actFlags = typeof data.actn === 'number' ? data.actn : 0;
    var actions = actionFlagsToText(actFlags);
    var alarmItems = codes.map(function(c) { return (ALARM_MSG[c] || ('Code ' + c)) + ' (码:' + c + ')'; });

    var html = '';
    if (linkLv === 2) {
      html += '<div class="linkage-toast-icon">🚨</div>';
      html += '<div class="linkage-toast-title">系统告警</div>';
    } else {
      html += '<div class="linkage-toast-icon">⚠️</div>';
      html += '<div class="linkage-toast-title">系统预警</div>';
    }
    html += '<div class="linkage-toast-body">';
    html += '<div class="linkage-toast-sensors">' + alarmItems.join('<br>') + '</div>';
    html += '<div class="linkage-toast-actions">已采取措施：<strong>' + (actions.length ? actions.join(' / ') : '--') + '</strong></div>';
    html += '</div>';

    container.innerHTML = html;
    container.className = 'linkage-toast linkage-toast--' + (linkLv === 2 ? 'alarm' : 'warn') + ' linkage-toast--show';

    toastTimer = setTimeout(function () {
      container.classList.remove('linkage-toast--show');
    }, 10000);
  }

  /* ===== 数据解析 ===== */
  function normalizePayload(raw) {
    if (!raw || typeof raw !== 'object') return raw;

    if (raw.type === 'telemetry' && raw.payload && typeof raw.payload === 'object') {
      raw = raw.payload;
    }

    // 新协议：sen/lv/act/mod/alm(数组)/actn(位掩码)/res
    if (raw.sen) {
      return {
        sen: { t: raw.sen.t, h: raw.sen.h, pm: raw.sen.pm, aq: raw.sen.aq, f: raw.sen.f, i: raw.sen.i },
        res: { bp: raw.res ? raw.res.bp : undefined, wp: raw.res ? raw.res.wp : undefined,
               tu: raw.res ? raw.res.tu : undefined, wu: raw.res ? raw.res.wu : undefined,
               br: raw.res ? raw.res.br : undefined, wr: raw.res ? raw.res.wr : undefined,
               ps: raw.res ? raw.res.ps : undefined,
               bt: raw.res ? raw.res.bt : undefined, wt: raw.res ? raw.res.wt : undefined },
        lv: raw.lv || { link: 0, env: 0, d: [0,0,0,0,0,0] },
        act: raw.act || {},
        mod: raw.mod || {},
        alm: Array.isArray(raw.alm) ? raw.alm : [],
        actn: typeof raw.actn === 'number' ? raw.actn : 0,
        th: raw.th || null
      };
    }

    // 兼容旧协议
    if (typeof raw.t === 'number' && typeof raw.h === 'number') {
      var lv = raw.lv || 0;
      return {
        sen: { t: raw.t, h: raw.h, pm: raw.p || raw.pm, aq: raw.a || raw.aq, f: raw.f, i: raw.i },
        res: { bp: raw.bp, wp: raw.wp, tu: raw.tu, wu: raw.wu },
        lv: { link: lv, env: lv, d: [lv,lv,lv,lv,lv,lv] },
        act: {}, mod: {}, alm: [], actn: 0
      };
    }

    return raw;
  }

  /* ===== 数据应用 ===== */
  function applyPayload(obj) {
    updateMetrics(obj);
    updateResources(obj);
    updateAlarms(obj);
    showLinkageToast(obj);
    var ft = document.getElementById('footerDataTime');
    if (ft) ft.textContent = '最近更新: ' + new Date().toLocaleString();
  }

  /* ---- 24 小时天气预报（保持不变） ---- */
  function initForecast() {
    var now = new Date();
    var currentHour = now.getHours();
    var weatherIcons = ['☀️','🌤️','⛅','☁️','🌧️'];

    var data = [];
    for (var i = 0; i <= 24; i++) {
      var h = (currentHour + i) % 24;
      var isNight = h < 6 || h >= 20;
      var tempBase = 24 + Math.sin((h - 8) / 5) * 6;
      var temp = tempBase + (Math.random() - 0.5) * 2;
      var wi = Math.floor(Math.random() * weatherIcons.length);
      wi = isNight ? (wi > 2 ? 2 : wi) : wi;
      var weather;
      if (i === 0) weather = '🌤️';
      else if (isNight && Math.random() < 0.55) weather = '🌙';
      else weather = weatherIcons[wi];
      var windStrong = Math.random() < 0.38;
      var pm = 25 + Math.sin(i / 8) * 20 + (Math.random() - 0.5) * 10;
      var airLv = pm < 35 ? 'good' : pm < 75 ? 'moderate' : 'poor';
      var airText = airLv === 'good' ? '优' : airLv === 'moderate' ? '良' : '差';
      var uvRoll = Math.random();
      var uvKey = uvRoll < 0.52 ? 'weak' : uvRoll < 0.82 ? 'mid' : 'strong';
      var uvLabel = uvKey === 'weak' ? '紫外线弱' : uvKey === 'mid' ? '紫外线中等' : '紫外线强';
      var dayOff = Math.floor((currentHour + i) / 24);
      var slotD = new Date(now.getFullYear(), now.getMonth(), now.getDate() + dayOff);
      var timeLabel;
      if (i === 0) timeLabel = '现在';
      else if (h === 0) timeLabel = pad2(slotD.getMonth() + 1) + '/' + pad2(slotD.getDate());
      else timeLabel = h + ':00';

      data.push({
        hour: h, label: timeLabel,
        temp: Math.round(temp), weather: weather,
        windStrong: windStrong, windLabel: windStrong ? '强风' : '微风',
        pm: Math.round(pm), airLv: airLv, airText: airText,
        uvKey: uvKey, uvLabel: uvLabel, isNow: i === 0,
      });
    }
    forecastData = data;
    applyWeatherBackground(data);
    renderTempLine(data);
    renderForecastCols(data);
    bindForecastActiveSync();
    startForecastTipRotation();
    var scroller = document.getElementById('forecastScroll');
    if (scroller) enableDragScroll(scroller);
    requestAnimationFrame(function () {
      scrollForecastToInitial();
      requestAnimationFrame(function () { syncForecastActiveCol(); });
    });
  }

  function scrollForecastToInitial() {
    var sc = document.getElementById('forecastScroll');
    var data = forecastData;
    if (!sc || !data || !data.length) return;
    var n = data.length;
    var W = forecastContentWidthPx(n);
    var c0 = forecastColBounds(0, n, W).centerX;
    var target = c0 - sc.clientWidth * 0.5;
    var maxScroll = Math.max(0, sc.scrollWidth - sc.clientWidth);
    sc.scrollLeft = Math.max(0, Math.min(maxScroll, target));
  }

  function getForecastColAtX(clientX) {
    var sc = document.getElementById('forecastScroll');
    if (!sc) return 0;
    var cols = sc.querySelectorAll('.fc-col');
    if (!cols.length) return 0;
    var best = 0, bestDist = Infinity;
    for (var i = 0; i < cols.length; i++) {
      var r = cols[i].getBoundingClientRect();
      var cx = (r.left + r.right) * 0.5;
      var d = Math.abs(clientX - cx);
      if (d < bestDist) { bestDist = d; best = i; }
    }
    return best;
  }

  function setForecastActiveCol(index) {
    var sc = document.getElementById('forecastScroll');
    if (!sc) return;
    var cols = sc.querySelectorAll('.fc-col');
    for (var j = 0; j < cols.length; j++) cols[j].classList.toggle('fc-col--active', j === index);
  }

  function syncForecastActiveCol() {
    var sc = document.getElementById('forecastScroll');
    if (!sc) return;
    var cols = sc.querySelectorAll('.fc-col');
    if (!cols.length) return;
    var rect = sc.getBoundingClientRect();
    var cx = rect.left + rect.width * 0.5;
    setForecastActiveCol(getForecastColAtX(cx));
  }

  function bindForecastActiveSync() {
    var sc = document.getElementById('forecastScroll');
    if (!sc || sc.dataset.activeSync === '1') return;
    sc.dataset.activeSync = '1';
    sc.dataset.hoverActive = 'false';
    function tick() { if (sc.dataset.hoverActive !== 'true') syncForecastActiveCol(); }
    function onMouseMove(e) { sc.dataset.hoverActive = 'true'; setForecastActiveCol(getForecastColAtX(e.clientX)); }
    function onMouseLeave() { sc.dataset.hoverActive = 'false'; syncForecastActiveCol(); }
    sc.addEventListener('scroll', tick, { passive: true });
    sc.addEventListener('mousemove', onMouseMove);
    sc.addEventListener('mouseleave', onMouseLeave);
    window.addEventListener('resize', tick);
  }

  function forecastContentWidthPx(pointCount) {
    return pointCount * FC_COL_WIDTH + Math.max(0, pointCount - 1) * FC_COL_GAP;
  }

  function forecastColBounds(i, n, W) {
    if (n <= 1) return { left: 0, width: W, centerX: W * 0.5 };
    function kx(j) { return (j / (n - 1)) * W; }
    var left = i === 0 ? 0 : (kx(i - 1) + kx(i)) * 0.5;
    var right = i === n - 1 ? W : (kx(i) + kx(i + 1)) * 0.5;
    var width = right - left;
    return { left: left, width: width, centerX: left + width * 0.5 };
  }

  function forecastWindLogoSvg(windStrong) {
    var angles = windStrong ? [-68, -24, 24, 68] : [-56, 0, 56];
    var leafD = 'M0 2 C-5.2 -7.5,-4.2 -13.5,0 -15.5 C5.2 -13.5,5.2 -7.5,0 2 Z';
    var parts = ['<svg class="fc-wind-svg" viewBox="0 0 40 40" xmlns="http://www.w3.org/2000/svg" aria-hidden="true"><g transform="translate(20,31)">'];
    for (var ai = 0; ai < angles.length; ai++) parts.push('<g transform="rotate(' + angles[ai] + ')"><path fill="currentColor" d="' + leafD + '"/></g>');
    parts.push('</g></svg>');
    return parts.join('');
  }

  function forecastUvSvg() {
    return '<svg class="fc-uv-svg" viewBox="0 0 36 36" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">' +
      '<circle cx="18" cy="13" r="5.5" fill="none" stroke="currentColor" stroke-width="1.6" opacity="0.95"/>' +
      '<path d="M18 6v2.2M12.2 8.8l1.6 1.6M24 8.8l-1.6 1.6M10 13h2.2M24 13h2.2" stroke="currentColor" stroke-width="1.2" stroke-linecap="round" opacity="0.85"/>' +
      '<path d="M7 20 Q18 14 29 20 L26 25 Q18 21 10 25Z" fill="currentColor" opacity="0.88"/>' +
      '<path d="M7 20 Q18 16 29 20" fill="none" stroke="rgba(255,255,255,0.35)" stroke-width="0.8"/></svg>';
  }

  function tempToStrokeColor(t, minT, maxT) {
    var u = (t - minT) / (maxT - minT);
    if (u < 0) u = 0; if (u > 1) u = 1;
    var h = 198 - u * 186, s = 72 + u * 22, l = 54 + Math.sin(u * Math.PI) * 7;
    return 'hsl(' + h.toFixed(1) + ',' + Math.round(s) + '%,' + Math.round(l) + '%)';
  }

  function renderTempLine(data) {
    var svg = document.getElementById('forecastTempSvg');
    var defs = document.getElementById('forecastTempDefs');
    var layer = document.getElementById('forecastTempSegments');
    var labelLayer = document.getElementById('forecastTempLabels');
    if (!svg || !defs || !layer || !labelLayer || !data.length) return;
    var NS = 'http://www.w3.org/2000/svg';
    var n = data.length, svgW = forecastContentWidthPx(n), svgH = 152;
    var padT = 10, padB = 12;
    var curveTop = padT, curveBottom = svgH - padB, drawH = curveBottom - curveTop;
    var temps = data.map(function (d) { return d.temp; });
    var minData = Math.min.apply(null, temps), maxData = Math.max.apply(null, temps);
    var span = Math.max(maxData - minData, 4);
    var minT = minData - span * 0.15, maxT = maxData + span * 0.15;
    if (maxT - minT < 6) { var mid = (maxT + minT) / 2; minT = mid - 3; maxT = mid + 3; }
    function yAtTemp(t) { return curveTop + drawH - ((t - minT) / (maxT - minT)) * drawH; }
    var knots = data.map(function (d, i) { var b = forecastColBounds(i, n, svgW); return { x: b.centerX, y: yAtTemp(d.temp), temp: d.temp }; });
    svg.setAttribute('width', String(svgW)); svg.setAttribute('height', String(svgH));
    defs.innerHTML = ''; layer.innerHTML = ''; labelLayer.innerHTML = '';
    var samples = [], STEPS = 22;
    function dup(p) { return { x: p.x, y: p.y }; }
    var ext = [dup(knots[0])].concat(knots).concat([dup(knots[n - 1])]);
    for (var seg = 0; seg < n - 1; seg++) {
      var p0 = ext[seg], p1 = ext[seg + 1], p2 = ext[seg + 2], p3 = ext[seg + 3];
      var bx0 = p1.x, by0 = p1.y;
      var bx1 = p1.x + (p2.x - p0.x) / 6, by1 = p1.y + (p2.y - p0.y) / 6;
      var bx2 = p2.x - (p3.x - p1.x) / 6, by2 = p2.y - (p3.y - p1.y) / 6;
      var bx3 = p2.x, by3 = p2.y;
      var t0 = data[seg].temp, t1 = data[seg + 1].temp;
      function bezPt(u) {
        var uc = 1 - u, uu = u * u, uuu = uu * u, c = uc * uc, cc = c * uc;
        return { x: cc * bx0 + 3 * c * u * bx1 + 3 * uc * uu * bx2 + uuu * bx3, y: cc * by0 + 3 * c * u * by1 + 3 * uc * uu * by2 + uuu * by3, temp: (1 - u) * t0 + u * t1 };
      }
      for (var s = 0; s < STEPS; s++) {
        var u0 = s / STEPS, u1 = (s + 1) / STEPS;
        var A = bezPt(u0), B = bezPt(u1);
        samples.push({ x0: A.x, y0: A.y, x1: B.x, y1: B.y, c0: tempToStrokeColor(A.temp, minT, maxT), c1: tempToStrokeColor(B.temp, minT, maxT) });
      }
    }
    var strokeW = 3.2;
    for (var k = 0; k < samples.length; k++) {
      var sm = samples[k];
      var gid = 'fcTempGrad' + k;
      var grad = document.createElementNS('http://www.w3.org/2000/svg', 'linearGradient');
      grad.setAttribute('id', gid); grad.setAttribute('gradientUnits', 'userSpaceOnUse');
      grad.setAttribute('x1', String(sm.x0)); grad.setAttribute('y1', String(sm.y0));
      grad.setAttribute('x2', String(sm.x1)); grad.setAttribute('y2', String(sm.y1));
      var s0 = document.createElementNS('http://www.w3.org/2000/svg', 'stop'); s0.setAttribute('offset', '0%'); s0.setAttribute('stop-color', sm.c0);
      var s1 = document.createElementNS('http://www.w3.org/2000/svg', 'stop'); s1.setAttribute('offset', '100%'); s1.setAttribute('stop-color', sm.c1);
      grad.appendChild(s0); grad.appendChild(s1); defs.appendChild(grad);
      var line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      line.setAttribute('x1', String(sm.x0)); line.setAttribute('y1', String(sm.y0));
      line.setAttribute('x2', String(sm.x1)); line.setAttribute('y2', String(sm.y1));
      line.setAttribute('stroke', 'url(#' + gid + ')'); line.setAttribute('stroke-width', String(strokeW)); line.setAttribute('stroke-linecap', 'round');
      layer.appendChild(line);
    }
    var labelR = 13;
    for (var j = 0; j < n; j++) {
      var kn = knots[j], cx = kn.x, cyLab = kn.y, strokeCol = tempToStrokeColor(kn.temp, minT, maxT);
      var g = document.createElementNS(NS, 'g');
      var circ = document.createElementNS(NS, 'circle');
      circ.setAttribute('cx', String(cx)); circ.setAttribute('cy', String(cyLab)); circ.setAttribute('r', String(labelR));
      circ.setAttribute('fill', '#FFFFFF'); circ.setAttribute('stroke', strokeCol); circ.setAttribute('stroke-width', '2.2');
      var tx = document.createElementNS(NS, 'text');
      tx.setAttribute('x', String(cx)); tx.setAttribute('y', String(cyLab));
      tx.setAttribute('text-anchor', 'middle'); tx.setAttribute('dominant-baseline', 'middle');
      tx.setAttribute('fill', '#0F172A'); tx.setAttribute('font-size', '11'); tx.setAttribute('font-weight', '800');
      tx.setAttribute('font-family', 'Segoe UI, PingFang SC, Microsoft YaHei, sans-serif');
      tx.textContent = String(Math.round(kn.temp));
      g.appendChild(circ); g.appendChild(tx); labelLayer.appendChild(g);
    }
    var wpx = svgW + 'px';
    var wrap = document.getElementById('forecastSvgWrap'), cols = document.getElementById('forecastCols');
    if (wrap) wrap.style.width = wpx; if (cols) cols.style.width = wpx;
  }

  function renderForecastCols(data) {
    var container = document.getElementById('forecastCols');
    if (!container) return;
    var n = data.length, W = forecastContentWidthPx(n);
    container.style.width = W + 'px';
    container.innerHTML = data.map(function (d, i) {
      var b = forecastColBounds(i, n, W);
      var timeClass = d.isNow ? 'fc-time fc-time--now' : 'fc-time';
      var windStackClass = 'fc-wind-stack' + (d.windStrong ? ' fc-wind-stack--strong' : '');
      return '<div class="fc-col" data-col-index="' + i + '" style="flex:0 0 ' + b.width + 'px;width:' + b.width + 'px">' +
        '<div class="fc-weather">' + d.weather + '</div>' +
        '<div class="' + windStackClass + '">' + forecastWindLogoSvg(d.windStrong) + '<span class="fc-wind-cap">' + d.windLabel + '</span></div>' +
        '<div class="fc-air fc-air--' + d.airLv + '"><span class="fc-air-ico">🍃</span><span class="fc-air-cap fc-air-cap--' + d.airLv + '">空气' + d.airText + '</span></div>' +
        '<div class="fc-uv fc-uv--' + d.uvKey + '">' + forecastUvSvg() + '<span class="fc-uv-cap">' + d.uvLabel + '</span></div>' +
        '<div class="' + timeClass + '">' + d.label + '</div></div>';
    }).join('');
  }

  function snapForecastToNearest() {
    var sc = document.getElementById('forecastScroll'); if (!sc) return;
    var cols = sc.querySelectorAll('.fc-col'); if (!cols.length) return;
    var rect = sc.getBoundingClientRect();
    var cx = rect.left + rect.width * 0.5, best = 0, bestD = Infinity;
    for (var i = 0; i < cols.length; i++) {
      var r = cols[i].getBoundingClientRect(), cc = (r.left + r.right) * 0.5, d = Math.abs(cc - cx);
      if (d < bestD) { bestD = d; best = i; }
    }
    var t = cols[best], tr = t.getBoundingClientRect(), colC = (tr.left + tr.right) * 0.5;
    var delta = colC - cx, maxS = Math.max(0, sc.scrollWidth - sc.clientWidth);
    sc.scrollLeft = Math.max(0, Math.min(maxS, sc.scrollLeft + delta));
    syncForecastActiveCol();
  }

  function enableDragScroll(container) {
    var inertiaRaf = null;
    function stopInertia() { if (inertiaRaf != null) { cancelAnimationFrame(inertiaRaf); inertiaRaf = null; } }
    function clientXLocal(e) { var ev = e.touches && e.touches[0] ? e.touches[0] : e; return ev.pageX - container.getBoundingClientRect().left; }
    var isDown = false, startX = 0, scrollLeft0 = 0, lastX = 0, lastT = 0, vel = 0;
    function onDown(e) { stopInertia(); isDown = true; startX = clientXLocal(e); scrollLeft0 = container.scrollLeft; lastX = startX; lastT = performance.now(); vel = 0; container.style.cursor = 'grabbing'; }
    function onMove(e) { if (!isDown) return; if (e.cancelable) e.preventDefault(); var x = clientXLocal(e), now = performance.now(), dt = Math.max(1, now - lastT); vel = vel * 0.38 + (x - lastX) / dt * 0.62; lastX = x; lastT = now; container.scrollLeft = scrollLeft0 - (x - startX) * 1.35; syncForecastActiveCol(); }
    function endInertiaOrSnap() { var vScroll = -vel * 22; function step() { if (Math.abs(vScroll) < 0.45) { inertiaRaf = null; snapForecastToNearest(); return; } container.scrollLeft += vScroll; vScroll *= 0.935; syncForecastActiveCol(); inertiaRaf = requestAnimationFrame(step); } if (Math.abs(vScroll) < 0.5) snapForecastToNearest(); else inertiaRaf = requestAnimationFrame(step); }
    function onUp() { if (!isDown) return; isDown = false; container.style.cursor = 'grab'; container.dataset.hoverActive = 'false'; endInertiaOrSnap(); }
    function onLeave() { if (!isDown) return; isDown = false; container.style.cursor = 'grab'; container.dataset.hoverActive = 'false'; endInertiaOrSnap(); }
    container.addEventListener('mousedown', onDown); container.addEventListener('touchstart', onDown, { passive: true });
    container.addEventListener('mousemove', onMove); container.addEventListener('touchmove', onMove, { passive: false });
    container.addEventListener('mouseup', onUp); container.addEventListener('touchend', onUp);
    container.addEventListener('touchcancel', onUp); container.addEventListener('mouseleave', onLeave);
  }

  function initTrendChart() {
    var ctx = document.getElementById('chartTrend'); if (!ctx) return;
    var histData = (window.HISTORY_DATA && window.HISTORY_DATA.recent7 && window.HISTORY_DATA.recent7.length >= 2) ? window.HISTORY_DATA.recent7 : [];
    var labels, powerData, waterData, powerUnit = 'Ah', waterUnit = 'L';
    if (histData) {
      labels = histData.map(function (d) { return d.date.slice(5); });
      powerData = histData.map(function (d) { return ((d.powerMAh || 0) / 1000); });
      waterData = histData.map(function (d) { return d.waterL || 0; });
    } else {
      labels = ['05/09','05/10','05/11','05/12','05/13','05/14','今日'];
      powerData = [12.3,11.8,13.1,12.5,10.9,11.2,12.0]; waterData = [1.8,1.6,2.1,1.9,1.5,1.7,1.4];
      powerUnit = 'kWh'; waterUnit = 'm³';
    }
    new Chart(ctx, {
      type: 'line', data: { labels: labels, datasets: [
        { label: '用电量 (' + powerUnit + ')', data: powerData, borderColor: '#F59E0B', backgroundColor: 'rgba(245,158,11,0.1)', fill: true, tension: 0.3, pointRadius: 3, yAxisID: 'y' },
        { label: '用水量 (' + waterUnit + ')', data: waterData, borderColor: '#3B82F6', backgroundColor: 'rgba(59,130,246,0.1)', fill: true, tension: 0.3, pointRadius: 3, yAxisID: 'y1' },
      ]},
      options: {
        responsive: true, maintainAspectRatio: false,
        interaction: { mode: 'index', intersect: false },
        plugins: { legend: { labels: { color: '#94A3B8', font: { size: 10 }, boxWidth: 12, padding: 8 } } },
        scales: {
          x: { ticks: { color: '#94A3B8', font: { size: 9 } }, grid: { color: 'rgba(255,255,255,0.03)' } },
          y: { position: 'left', ticks: { color: '#F59E0B', font: { size: 9 } }, grid: { color: 'rgba(255,255,255,0.05)' }, title: { display: true, text: powerUnit, color: '#94A3B8' } },
          y1: { position: 'right', ticks: { color: '#3B82F6', font: { size: 9 } }, grid: { display: false }, title: { display: true, text: waterUnit, color: '#94A3B8' } },
        }
      }
    });
  }

  /* ===== MQTT ===== */
  function setConnStatus(on) {
    var dot = document.getElementById('connDot'), label = document.getElementById('connLabel');
    if (!dot || !label) return;
    dot.className = 'data-dot' + (on ? ' data-dot--on' : '');
    label.textContent = on ? '数据实时更新中' : '已断开';
  }

  function connectMqtt() {
    if (typeof mqtt === 'undefined') return;
    var url = CFG.mqttUrl || ''; if (!url) { setConnStatus(false); return; }
    var opts = { clientId: CFG.clientId || 'webdash_' + Math.random().toString(16).slice(2, 10), clean: true, reconnectPeriod: 3000, connectTimeout: 10000 };
    if (CFG.username) opts.username = CFG.username;
    if (CFG.password) opts.password = CFG.password;
    mqttClient = mqtt.connect(url, opts);
    mqttClient.on('connect', function () {
      setConnStatus(true);
      var topic = CFG.topic || '#';
      mqttClient.subscribe(topic, function (err) { if (err) console.error('订阅失败', err); });
    });
    mqttClient.on('message', function (_topic, message) {
      try {
        var s = typeof message === 'string' ? message : new TextDecoder().decode(message);
        var obj = JSON.parse(s);
        if (!obj || typeof obj !== 'object') return;
        applyPayload(normalizePayload(obj));
      } catch (e) {}
    });
    mqttClient.on('error', function () { setConnStatus(false); });
    mqttClient.on('close', function () { setConnStatus(false); });
    mqttClient.on('offline', function () { setConnStatus(false); });
  }

  function publishHelpWrapped(type, label) {
    if (!mqttClient || !mqttClient.connected) return;
    var helpTopic = CFG.helpTopic || 'help';
    mqttClient.publish(helpTopic, JSON.stringify({
      type: 'help',
      source: 'web',
      payload: {
        type: type,
        label: label,
        time: new Date().toISOString(),
        site: CFG.siteName || '一号营地'
      }
    }));
  }

  function initHelpButton() {
    var floatEl = document.getElementById('helpFloat'), btn = document.getElementById('helpBtn'), cancel = document.getElementById('helpCancel'), toast = document.getElementById('helpToast'), options = document.querySelectorAll('.help-option');
    if (!floatEl || !btn) return;
    var isOpen = false, toastTimer = null;
    function open() { isOpen = true; floatEl.classList.add('help-float--open'); }
    function close() { isOpen = false; floatEl.classList.remove('help-float--open'); }
    btn.addEventListener('click', function (e) { e.stopPropagation(); isOpen ? close() : open(); });
    if (cancel) cancel.addEventListener('click', function (e) { e.stopPropagation(); close(); });
    options.forEach(function (opt) {
      opt.addEventListener('click', function (e) {
        e.stopPropagation(); var type = opt.dataset.type;
        var text = (opt.querySelector('.help-option-text') || {}).textContent || type;
        publishHelpWrapped(type, text); close();
        if (toast) { if (toastTimer) clearTimeout(toastTimer); toast.classList.add('help-toast--show'); toastTimer = setTimeout(function () { toast.classList.remove('help-toast--show'); }, 3000); }
      });
    });
    document.addEventListener('click', function (e) { if (isOpen && !floatEl.contains(e.target)) close(); });
  }

  function applyWeatherBackground(data) {
    if (!data || !data.length) return;
    var wrap = document.querySelector('.forecast-wrap'); if (!wrap) return;
    var counts = { sunny: 0, cloudy: 0, rainy: 0, night: 0 };
    var sunnyEmojis = ['☀️','🌤️'], cloudyEmojis = ['⛅','☁️'], rainyEmojis = ['🌧️'], nightEmojis = ['🌙'];
    for (var i = 0; i < Math.min(data.length, 8); i++) {
      var w = data[i].weather;
      if (sunnyEmojis.indexOf(w) >= 0) counts.sunny++;
      else if (cloudyEmojis.indexOf(w) >= 0) counts.cloudy++;
      else if (rainyEmojis.indexOf(w) >= 0) counts.rainy++;
      else if (nightEmojis.indexOf(w) >= 0) counts.night++;
    }
    var maxKey = 'sunny', maxCount = 0;
    for (var k in counts) { if (counts[k] > maxCount) { maxCount = counts[k]; maxKey = k; } }
    wrap.className = 'forecast-wrap forecast-wrap--' + maxKey;
  }

  function startForecastTipRotation() {
    var el = document.getElementById('forecastTipCarousel'); if (!el) return;
    function show() { el.textContent = FORECAST_TIPS[forecastTipIndex % FORECAST_TIPS.length]; forecastTipIndex++; }
    show(); setInterval(show, 6500);
  }

  function startTipRotation() { setInterval(function () { tipIndex = (tipIndex + 1) % TIPS.length; }, 8000); }

  function clockTick() {
    var el = document.getElementById('clock'); if (!el) return;
    var d = new Date();
    el.textContent = d.getFullYear() + '-' + pad2(d.getMonth()+1) + '-' + pad2(d.getDate()) + '  ' + pad2(d.getHours()) + ':' + pad2(d.getMinutes()) + ':' + pad2(d.getSeconds());
  }
  function pad2(n) { return (n < 10 ? '0' : '') + n; }

  /* ===== 初始化 ===== */
  function init() {
    clockTick(); setInterval(clockTick, 1000);
    connectMqtt();
    initForecast();
    initTrendChart();
    initHelpButton();
    startTipRotation();
    var resizeTimer = null;
    window.addEventListener('resize', function () {
      if (resizeTimer) clearTimeout(resizeTimer);
      resizeTimer = setTimeout(function () {
        if (forecastData && forecastData.length) { renderTempLine(forecastData); renderForecastCols(forecastData); syncForecastActiveCol(); }
      }, 200);
    });
    setInterval(function () { if (mqttClient && !mqttClient.connected) setConnStatus(false); }, 30000);
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init);
  else init();
})();
