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

    this.setupCanvas();
    this.play();

    const interval = 3.0;
    setInterval(()=>{
      this.speed = this.smooth(this.speed, this.bytes / interval);
      this.bytes = 0;
      this.fps = this.smooth(this.fps, this.frameCount / interval);
      this.frameCount = 0;
    }, interval*1000);
  }

  smooth(oldVal, newVal) {
    if (!this.isPlaying) return 0;
    return this.alpha * newVal + (1-this.alpha) * oldVal;
  }

  destroy() {
    this.isPlaying = false;
    this.ws.close(1000, 'close'); 
  }

  setupWs() {
    this.ws = new WebSocket(this.url);
    this.ws.onopen = () => {
      console.log('WebSocket Connected', this.url);
      this.nextFrame(this.full);
    };

    this.ws.onmessage = (event) => {
      // console.log('Received type:', typeof event.data);
      if (typeof event.data === 'string') {
        console.log('Received text:', event.data);
      } else if (event.data instanceof Blob) {
        // 二进制数据（Blob 类型）
        // console.log('收到二进制消息（Blob），大小:', event.data.size);

        if (this.isPlaying) {
          this.drawBlob(event.data);
          this.bytes += event.data.size;
          this.delay = this.smooth(this.delay, new Date().getTime() - this.delayTimestamp);

          this.nextFrame(this.full); // todo
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
    // 优化图像渲染设置
    // this.ctx.imageSmoothingEnabled = false;
    this.ctx.globalCompositeOperation = 'source-over';

    // 设置填充样式
    this.ctx.fillStyle = '#000000';

    // 设置初始尺寸
    this.resizeCanvas();

    // 监听窗口和容器尺寸变化
    window.addEventListener('resize', () => this.resizeCanvas());

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

    // console.log(`高清 Canvas: ${displayWidth}x${displayHeight} @ ${this.pixelRatio}x`);
  }

  resizeCanvas() {
    this.setupHighDPICanvas();
    if (!this.isPlaying) {
      this.drawPlay();
    }
  }

  nextFrame(type) {
    if (!this.isPlaying) {
      this.drawPlay();
      return;
    }
    this.delayTimestamp = new Date().getTime();
    try {
      if (this.ws.readyState !== WebSocket.OPEN) {
        console.warn("WS not ready:", this.ws.readyState);
        // this.setupWs();
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
    this.isPlaying = true;
    this.setupWs();
  }

  pause() {
    this.ws.close(1000, 'pause');
    this.stop();
    console.log("Pause");
  }

  stop() {
    this.drawPlay();
    this.isPlaying = false;
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
      this.drawPlay();
      return;
    }
    // 清除画布
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
  }

  drawText(text) {
    const width = this.canvas.width / this.pixelRatio;
    const height = this.canvas.height / this.pixelRatio;

    this.ctx.fillStyle = `hsl(${Date.now() / 10 % 360}, 70%, 50%)`;
    this.ctx.fillRect(0, height / 3, width, height / 3);

    this.ctx.fillStyle = '#fff';
    this.ctx.font = '20px Arial';
    this.ctx.textAlign = 'center';
    this.ctx.fillText(text, width / 2, height / 2);
    this.ctx.fillText(new Date().toLocaleTimeString(), width / 2, height / 2 + 30);
  }

  drawPlay() {
    const width = this.canvas.width / this.pixelRatio;
    const height = this.canvas.height / this.pixelRatio;

    const triWidth = 34; // 三角形宽度
    const triHeight = 42; // 三角形高度
    // 计算位置（居中显示）
    const x = width / 2 - triWidth / 4; // 向左微调一点更居中
    const y = height / 2 - triHeight / 2;

    // const dw = 12;
    // const dh = 20;
    // this.drawTriangle(x-dw/2, y-dh/2, triWidth+dw, triHeight+dh);
    // this.ctx.fillStyle = 'black';
    // this.ctx.fill();
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

var player;

function playOrPause() {
  if (player.isPlaying) {
    player.pause();
  } else {
    player.play();
  }
}

const alertBox = document.getElementById('alertBox');
const alertText = document.getElementById('alertText');
const closeBtn = document.getElementById('closeBtn');

function hideAlert() {
  alertBox.style.display = 'none';
}

closeBtn.addEventListener('click', hideAlert);

function showAlert(message) {
    alertText.textContent = message;
    alertBox.style.display = 'block';

    setTimeout(() => {
        hideAlert();
    }, 3000);
}

(function init() {
  const canvas = document.getElementById('video');
  canvas.onclick = (e)=>playOrPause();
  let host = window.location.host || "localhost:2333";
  player = new WsJpgPlayer(canvas, 'ws://' + host + '/stream');
  player.notify = (msg)=>{
    if (msg) {
      showAlert(msg);
    } else {
      showAlert("连接失败，请重试！");
    }
  };

  window.addEventListener('beforeunload', function(e) {
    player.ws.close(1000, 'page unload');
  });

  setInterval(() => {
    let fps = player.getFPS();
    fps = fps<0.1 ? '0' : fps.toFixed(2);
    document.getElementById('fps').textContent = `${fps} fps`;

    const delay = player.getDelay();
    document.getElementById('delay').textContent = `${delay} ms`;

    const speed = player.getSpeed();
    document.getElementById('speed').textContent = `${speed}`;

    const state = player.isPlaying? '▶︎' : '🆇';
    document.getElementById('state').textContent = `${state}`;

  }, 1000);
})();