class WsJpgPlayer {
  constructor(canvas, url) {
    this.full = "STREAM:FULL";
    this.diff = "STREAM:DIFF";
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d', {
      alpha: false,
    });
    this.url = url;
    this.isPlaying = false;

    this.frameCount = 0;
    // this.fpsTimestamp = new Date().getTime();
    this.fps = 0;
    this.delayTimestamp = new Date().getTime();
    this.delay = 0;
    this.bytes = 0;
    this.speed = 0;
    this.alpha = 0.6;
    this.notify;
    this.onStop = null;
    this.onStart = null;

    this.setupCanvas();
    this.setupCursor();

    const interval = 3.0;
    setInterval(()=>{
      this.speed = this.smooth(this.speed, this.bytes / interval);
      this.bytes = 0;
      this.fps = this.smooth(this.fps, this.frameCount / interval);
      this.frameCount = 0;
    }, interval*1000);

  }

  setupCursor() {
    const svgString = `<svg xmlns="http://www.w3.org/2000/svg" x="0px" y="0px" width="100" height="100" viewBox="0 0 48 48">
<path fill="#90a4ae" d="M36.149,25.242L18.579,9.055V7h-3.11c-0.065,0.003-2.192-0.003-2.192,2.182v25.836	c0,1.754,1.523,2.107,2.114,2.109h3.184v-2.08l2.649-2.43l3.9,8.918c0.367,0.92,1.264,1.403,2.153,1.447V43h3v-1.06l1.518-0.682	c0.528-0.234,0.924-0.654,1.116-1.185c0.203-0.559,0.156-1.179-0.117-1.715l-4.06-8.697l6.096-0.571l0.152-0.026	c1.292-0.317,1.699-1.125,1.828-1.579C37.023,26.727,36.774,25.856,36.149,25.242z"></path><path fill="#e0e0e0" stroke="#37474f" stroke-miterlimit="10" stroke-width="2" d="M29.137,41.156l-4.482-10.248l-5.377,4.932	c-0.656,0.559-1.912,0.307-1.912-0.822V9.182c0-1.154,1.325-1.516,2.031-0.822l19.12,17.616c0.576,0.566,0.723,1.76-0.717,2.114	l-7.528,0.705l4.66,9.982c0.33,0.648,0.082,1.311-0.489,1.565l-3.574,1.606C30.298,42.152,29.418,41.884,29.137,41.156z"></path>
</svg>`;
    const cursorUrl = "data:image/svg+xml;charset=utf-8," + encodeURIComponent(svgString);
    this.rx = 0;
    this.ry = 0;
    this.cursorImg = new Image();
    this.cursorImg.src = cursorUrl;
    this.cursorSize = 20;
  }

  stopPlaying() {
    this.isPlaying = false;
    if (this.onStop) this.onStop();
  }

  smooth(oldVal, newVal) {
    if (!this.isPlaying) return 0;
    return this.alpha * newVal + (1-this.alpha) * oldVal;
  }

  destroy() {
    this.stopPlaying();
    this.ws.close(1000, 'close'); 
  }

  setupWs() {
    this.ws = new WebSocket(this.url);

    this.timer = setTimeout(()=>{
      if (!this.isPlaying) {
        this.notify();
      }
    }, 5000);

    this.ws.onopen = () => {
      clearTimeout(this.timer);
      this.isPlaying = true;
      if (this.onStart) this.onStart();
      // console.log('WebSocket Connected', this.url);
      this.nextFrame(this.full);
    };

    this.ws.onmessage = (event) => {
      // console.log('Received type:', typeof event.data);
      if (typeof event.data === 'string') {
        // console.log('Received text:', event.data);
        try {
          const obj = JSON.parse(event.data);
          // console.log(obj);
          this.rx = obj.x / obj.w;
          this.ry = obj.y / obj.h;
          this.connections = obj.connections;
          this.clients = obj.clients;
        } catch (err) {
          console.log(err);
          console.log(event.data);
        }
      } else if (event.data instanceof Blob) {
        // 二进制数据（Blob 类型）
        // console.log('收到二进制消息（Blob），大小:', event.data.size);

        if (this.isPlaying) {
          this.drawBlob(event.data);
          this.bytes += event.data.size;
          this.delay = this.smooth(this.delay, new Date().getTime() - this.delayTimestamp);

          this.nextFrame(this.full);
        }
      }
    };

    this.ws.onerror = (event) => {
      console.warn('WebSocket error:', event);
      this.stop();
      this.notify();
    };

    this.ws.onclose = (event) => {
      console.log('WS Disconnected. Code:', event.code, 'Reason:', event.reason);
      this.stop();
      if (event.code !== 1000) {
        this.notify('连接已关闭!');
      }
    };

  }

  setupCanvas() {
    // this.ctx.imageSmoothingEnabled = false;
    this.ctx.globalCompositeOperation = 'source-over';

    this.ctx.fillStyle = '#000000';

    const resizeCanvas = () => {
      this.setupHighDPICanvas();
      if (!this.isPlaying) {
        this.drawPause();
      }
    }
    resizeCanvas();

    window.addEventListener('resize', () => resizeCanvas());
  }

  setupHighDPICanvas() {
    const container = this.canvas.parentElement;
    this.pixelRatio = Math.min(2, window.devicePixelRatio || 1);

    const displayWidth = container.clientWidth;
    const displayHeight = container.clientHeight;

    // 设置 Canvas 尺寸
    this.canvas.width = Math.round(displayWidth * this.pixelRatio);
    this.canvas.height = Math.round(displayHeight * this.pixelRatio);
    this.canvas.style.width = displayWidth + 'px';
    this.canvas.style.height = displayHeight + 'px';

    // 缩放上下文
    this.ctx.scale(this.pixelRatio, this.pixelRatio);
  }

  nextFrame(type) {
    if (!this.isPlaying) {
      this.drawPause();
      return;
    }

    this.delayTimestamp = new Date().getTime();
    try {
      if (this.ws.readyState !== WebSocket.OPEN) {
        console.warn("WS not ready:", this.ws.readyState);
      } else {
        this.ws.send(type);
      }
    } catch (e) {
      console.warn('Send failed:', e);
      this.notify();
    }
  }

  play() {
    console.log("Play");
    this.setupWs();
  }

  pause() {
    this.ws.close(1000, 'pause');
    this.stop();
    console.log("Pause");
  }

  stop() {
    this.drawPause();
    this.stopPlaying();
    this.delay = 0;
  }

  getFPS() {
    return this.fps;
  }

  getDelay() {
    return Math.round(this.delay);
  }

  getSpeed() {
    const KB = 1024;
    const MB = KB * 1024;
    if (this.speed > MB) {
      let t = (this.speed / MB).toFixed(2);
      return `${t} MB/s`
    } else if (this.speed > KB) {
      let t = (this.speed / KB).toFixed(2);
      return `${t} KB/s`
    } else {
      let t = (this.speed / KB).toFixed(0);
      return `${t} B/s`
    }
  }

  async drawBlob(blob) {
    try {
      const imageBitmap = await createImageBitmap(blob);
      this.drawFrame(imageBitmap);
      imageBitmap.close();
    } catch (err) {
      console.warn('Draw Blob failed:', err);
    }

    this.frameCount++;
  }

  drawFrame(image) {
    if (!this.isPlaying) {
      this.drawPause();
      return;
    }

    this.ctx.clearRect(0, 0, this.canvas.width / this.pixelRatio, this.canvas.height / this.pixelRatio);

    const canvasWidth = this.canvas.width / this.pixelRatio;
    const canvasHeight = this.canvas.height / this.pixelRatio;

    // 计算最佳绘制尺寸（保持比例）
    const imageAspect = image.width / image.height;
    const canvasAspect = canvasWidth / canvasHeight;

    let drawWidth, drawHeight, drawX, drawY;

    if (imageAspect > canvasAspect) {
      // 图像更宽，以宽度为准
      drawWidth = canvasWidth;
      drawHeight = drawWidth / imageAspect;
      drawX = 0;
      drawY = (canvasHeight - drawHeight) / 2;
    } else {
      // 图像更高，以高度为准
      drawHeight = canvasHeight;
      drawWidth = drawHeight * imageAspect;
      drawX = (canvasWidth - drawWidth) / 2;
      drawY = 0;
    }

    this.ctx.drawImage(image, drawX, drawY, drawWidth, drawHeight);

    const x = drawX + drawWidth * this.rx - this.cursorSize/4;
    const y = drawY + drawHeight * this.ry - this.cursorSize/4;
    this.ctx.drawImage(this.cursorImg, x, y, this.cursorSize, this.cursorSize); 
  }

  drawPause() {
    return;
    const width = this.canvas.width / this.pixelRatio;
    const height = this.canvas.height / this.pixelRatio;

    const triWidth = 34; // 三角形宽度
    const triHeight = 42; // 三角形高度
    // 计算位置（居中显示）
    const x = width / 2 - triWidth / 4; // 向左微调一点更居中
    const y = height / 2 - triHeight / 2;

    this.ctx.fillStyle = 'rgba(100, 100, 100, 0.5)';
    this.ctx.fillRect(0, 0, width, height);

    this.drawTriangle(x, y, triWidth, triHeight);
    this.ctx.fillStyle = `hsl(${Date.now() / 10 % 360}, 70%, 50%)`;
    this.ctx.fill();
  }

  drawTriangle(x, y, width, height) {
    this.ctx.beginPath();
    this.ctx.moveTo(x, y); // 左上角
    this.ctx.lineTo(x + width, y + height / 2); // 右下角（顶点）
    this.ctx.lineTo(x, y + height); // 左下角
    this.ctx.closePath();
  }
}

function initFullScreen() {
  const body = document.body;
  const toggleBtn = document.getElementById('toggleBtn');
  function toggleFullscreen() {
      if (!document.fullscreenElement) {
        if (body.requestFullscreen) {
          body.requestFullscreen().catch(err => {
              alert(`fullscreen failed: ${err.message}`);
          });
        } else {
          console.warn("requestFullscreen method not found!");
        }
      } else {
          if (document.exitFullscreen) {
              document.exitFullscreen();
          }
      }
  }

  document.addEventListener('fullscreenchange', () => {
      if (document.fullscreenElement) {
          toggleBtn.textContent = '🅧';
      } else {
          toggleBtn.textContent = '▣';
      }
  });

  toggleBtn.addEventListener('click', toggleFullscreen);
}

(function init() {
  initFullScreen();

  const alertBox = document.getElementById('alertBox');
  const alertText = document.getElementById('alertText');
  const closeBtn = document.getElementById('closeBtn');
  const pauseBtn = document.getElementById('pauseBtn');
  const canvas = document.getElementById('video');

  let url = '/stream';
  if (! window.location.host) {
    url = "ws://localhost:2333/stream";
  }
  const player = new WsJpgPlayer(canvas, url);

  const hideAlert = () => { alertBox.style.display = 'none'; };

  const showAlert = (message) => {
    alertText.textContent = message;
    alertBox.style.display = 'block';
    setTimeout(() => {hideAlert();}, 3000);
  };

  canvas.onclick = (e)=>{
    if (!player.isPlaying) {
      player.play();
    }
  };

  player.notify = (msg)=>{
    if (msg) {
      showAlert(msg);
    } else {
      showAlert("连接失败，请重试！");
    }
  };

  window.addEventListener('beforeunload', function() {
    player.pause();
  });

  closeBtn.addEventListener('click', hideAlert);

  pauseBtn.addEventListener("click", function(){
    if (player.isPlaying) {
      player.pause();
      pauseBtn.classList.add('play');
      pauseBtn.classList.remove('pause');
    } else {
      player.play();
      pauseBtn.classList.add('pause');
      pauseBtn.classList.remove('play');
    }
  });

  const play_container = document.getElementById('play-container')
  play_container.addEventListener("click", function(){
    player.play();
  });

  player.onStart = ()=>{
      pauseBtn.classList.add('pause');
      pauseBtn.classList.remove('play');
      play_container.classList.add('hidden');
  };

  player.onStop = ()=>{
      pauseBtn.classList.add('play');
      pauseBtn.classList.remove('pause');
      play_container.classList.remove('hidden');
      player.clients = [];
  };

  let update_details = ()=>{
    if (player.clients) {
      let s = '<ul>';
      for (const item of player.clients) {
        s += `<li>${item}</li>`;
      }
      s += '</ul>';
      details.innerHTML = s;
    }
  };

  const details = document.getElementById('details');
  details.addEventListener('mouseleave', ()=>{
    details.classList.add('hidden');
  });

  const info_fps = document.getElementById('fps');
  const info_delay = document.getElementById('delay');
  const info_speed = document.getElementById('speed');
  const info_state = document.getElementById('state');
  const info_clients = document.getElementById('clients');
  info_clients.addEventListener('mouseenter', ()=>{
    update_details();
    details.classList.remove('hidden');
  });

  let clients = 0;

  setInterval(() => {
    let fps = player.getFPS();
    fps = player.isPlaying ? fps.toFixed(2) : '0';
    info_fps.textContent = `${fps} fps`;

    const delay = player.getDelay();
    info_delay.textContent = `${delay} ms`;

    const speed = player.getSpeed();
    info_speed.textContent = `${speed}`;

    const state = player.isPlaying? '▶︎' : '🆇';
    info_state.textContent = `${state}`;

    if (clients != player.connections) {
      update_details();
    }
    clients = player.isPlaying ? player.connections : '?';
    info_clients.textContent = `${clients}`;

  }, 1000);

  player.play();

})();