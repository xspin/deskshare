globalThis.g_debug = false;

class WsPlayer {
    constructor(video, canvas, url) {
        this.ping = "PING";
        this.pong = "PONG";
        this.canvas = canvas;
        this.video = video;
        this.ctx = canvas.getContext('2d', {
            // alpha: false,
        });
        this.url = url;
        this.isPlaying = false;

        this.frameCount = 0;
        this.fps = 0;
        this.delayTimestamp = new Date().getTime(); // ms
        this.delay = 0;
        this.bytes = 0;
        this.speed = 0;
        this.alpha = 0.6;
        this.notify;
        this.onStop = null;
        this.onStart = null;
        this.ish264 = false;
        this.frame_width = 0;
        this.frame_height = 0;
        this.lastFrameTime = null;

        this.setupCanvas();
        this.setupCursor();

        const interval = 3.0;
        setInterval(() => {
            this.speed = this.smooth(this.speed, this.bytes / interval);
            this.bytes = 0;
            this.fps = this.smooth(this.fps, this.frameCount / interval);
            this.frameCount = 0;
        }, interval * 1000);

        this.jmuxer = new JMuxer({
            node: this.video,
            mode: 'video',
            flushingTime: 0,
            // fps: 20,
            // debug: true,
            onError: function(err) {
                console.error('JMuxer error:', err);
            }
        });

        // this.h264decoder = new VideoDecoder({
        //     output: (frame) => {
        //         this.drawFrame(frame, frame.displayWidth, frame.displayHeight);
        //         frame.close();
        //     },
        //     error: (e) => console.error('Failed to decode:', e)
        // });
        // this.h264decoder.configure({
        //     codec: 'avc1.42E01E',  // 必须匹配你的流
        // });
    }

    debug(...args) {
        if (g_debug) {
            console.log(...args);
        }
    }

    enableJpegDecoder() {
        console.log("Enable JPEG decoder");
        this.ish264 = false;
    }

    // onVideoFrame = (now, metadata) => {
    //     if (this.hiddenVideo.readyState >= 2) {
    //         // this.ctx.drawImage(this.hiddenVideo, 0, 0, this.canvas.width, this.canvas.height);
    //         this.drawFrame(this.hiddenVideo, this.frame_width, this.frame_height);
    //     }
    //     if (this.ish264) {
    //         this.hiddenVideo.requestVideoFrameCallback(this.onVideoFrame);
    //     }
    // }

    enableH264Decoder() {
        console.log("Enable H264 decoder");
        this.ish264 = true;
    }

    feedH264Frame(data) {
        const frame = new Uint8Array(data);
        if (frame.length == 0) {
            console.log("empty frame:", data);
            return;
        }

        // const now = performance.now();
        // const duration = this.lastFrameTime === null ? 100 : now - this.lastFrameTime;
        // this.lastFrameTime = now;
        // console.log(this.ws.binaryType, data.byteLength, data.size, data.length);
        this.jmuxer.feed({ video: frame, isLastVideoFrameComplete: true });

        // draw cursor
        this.drawFrame(null, this.frame_width, this.frame_height);

        // this.h264decoder.decode(new EncodedVideoChunk({
        //     type: this.key_frame ? 'key' : 'delta',
        //     timestamp: performance.now() * 1000,
        //     data: frame
        // }));
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
        this.lastFrameTime = null;
        this.isPlaying = false;
        if (this.onStop) this.onStop();
    }

    smooth(oldVal, newVal) {
        if (!this.isPlaying) return 0;
        return this.alpha * newVal + (1 - this.alpha) * oldVal;
    }

    destroy() {
        this.stopPlaying();
        this.ws.close(1000, 'close');
    }

    setupWs() {
        this.ws = new WebSocket(this.url);

        this.timer = setTimeout(() => {
            if (!this.isPlaying) {
                this.notify();
            }
        }, 5000);

        this.ws.onopen = () => {
            clearTimeout(this.timer);
            this.isPlaying = true;
            if (this.onStart) this.onStart();
            this.debug('WebSocket Connected', this.url);
            this.ws_keepalive = setInterval(() => {
                if (!this.nextFrame(this.ping)) {
                    clearInterval(this.ws_keepalive);
                    this.debug('clear keepalive interval', this.ws_keepalive);
                    this.ws_keepalive = null;
                }
            }, 3000);
            this.debug('start keepalive interval', this.ws_keepalive);
        };

        this.ws.onmessage = (event) => {
            const sz = event.data.size || event.data.byteLength || event.data.length || 0;
            this.bytes += sz;

            this.debug('Received type:', event.data.constructor.name, ', size:', sz);

            if (typeof event.data === 'string') {
                this.debug('Received text:', event.data);
                try {
                    if (event.data === this.pong) {
                        this.debug('Received PONG', 'delay:', this.delay, 'ms');
                        return;
                    }
                    const obj = JSON.parse(event.data);
                    // console.log(obj);
                    this.rx = obj.x / obj.w;
                    this.ry = obj.y / obj.h;
                    this.connections = obj.connections;
                    this.clients = obj.clients;
                    this.frame_width = obj.w;
                    this.frame_height = obj.h;
                    this.key_frame = obj.keyFrame;
                    if (!this.ish264 && obj.h264) {
                        this.enableH264Decoder()
                        this.ws.binaryType = "arraybuffer";
                    } else if (this.ish264 && !obj.h264) {
                        this.enableJpegDecoder();
                        this.ws.binaryType = "blob";
                    }
                } catch (err) {
                    console.log(err);
                    console.log(event.data);
                }
                const t = new Date().getTime();
                this.delay = this.smooth(this.delay, (t - this.delayTimestamp));
                this.delayTimestamp = t;
            } else if (event.data instanceof ArrayBuffer) {
                if (this.ish264) {
                    this.feedH264Frame(event.data);
                    this.frameCount++;
                }
            } else if (event.data instanceof Blob) {
                // 二进制数据（Blob 类型）
                // console.log('收到二进制消息（Blob），大小:', event.data.size);

                if (this.isPlaying) {
                    // console.log(this.ish264, event.data.byteLength, event.data.size, event.data.length);
                    if (this.ish264) {
                        event.data.arrayBuffer().then(data => {
                            this.feedH264Frame(data);
                        });
                        // h264 data
                    } else {
                        // jpeg data
                        this.drawBlobAsync(event.data);
                    }
                    this.frameCount++;
                }
            } else {
                console.log('recive unknown data type', typeof (event.data));
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

        if (g_debug) {
            console.log('display size:', displayWidth, displayHeight);
            console.log('video size:', this.video.videoWidth, this.video.videoHeight);
            console.log('canvas size:', this.canvas.width, this.canvas.height);
        }
    }

    nextFrame(data) {
        if (!this.isPlaying) {
            this.drawPause();
            return false;
        }

        try {
            if (this.ws.readyState !== WebSocket.OPEN) {
                console.warn("WS not ready:", this.ws.readyState);
            } else {
                this.debug('ws send:', data)
                this.ws.send(data);
                return true;
            }
        } catch (e) {
            console.warn('Send failed:', e);
            this.notify();
        }
        return false;
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
        this.frameCount = 0;
        this.bytes = 0;
    }

    getFPS() {
        return this.fps;
    }

    getDelay() {
        return this.delay > 1 ? Math.round(this.delay) : this.delay.toFixed(2);
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

    async drawBlobAsync(blob) {
        try {
            const imageBitmap = await createImageBitmap(blob);
            this.drawFrame(imageBitmap, imageBitmap.width, imageBitmap.height);
            imageBitmap.close();
        } catch (err) {
            console.warn('Draw Blob failed:', err);
        }

    }

    drawFrame(image, width, height) {
        if (!this.isPlaying) {
            this.drawPause();
            return;
        }

        const canvas_width = this.canvas.width;
        const canvas_height = this.canvas.height;

        this.ctx.clearRect(0, 0, canvas_width / this.pixelRatio, canvas_height / this.pixelRatio);

        const canvasWidth = canvas_width / this.pixelRatio;
        const canvasHeight = canvas_height / this.pixelRatio;

        // 计算最佳绘制尺寸（保持比例）
        const imageAspect = width / height;
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

        if (image) {
            this.ctx.drawImage(image, drawX, drawY, drawWidth, drawHeight);
        }

        if (this.rx > 0 || this.ry > 0) {
            const x = drawX + drawWidth * this.rx - this.cursorSize / 4;
            const y = drawY + drawHeight * this.ry - this.cursorSize / 4;
            this.ctx.drawImage(this.cursorImg, x, y, this.cursorSize, this.cursorSize);
        }
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

    getDecoderName() {
        if (!this.isPlaying || this.ish264 == null) return "";
        return (this.ish264 ? "H.264" : "MJPEG") + ` ${this.frame_width}x${this.frame_height}`;
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
    const video = document.getElementById('video');
    const canvas = document.getElementById('overlay');

    let url;
    if (!window.location.host) {
        url = "ws://localhost:2333/stream";
    } else {
        url = 'ws://' + window.location.host + '/stream'
    }
    globalThis.player = new WsPlayer(video, canvas, url);

    const hideAlert = () => { alertBox.style.display = 'none'; };

    const showAlert = (message) => {
        alertText.textContent = message;
        alertBox.style.display = 'block';
        setTimeout(() => { hideAlert(); }, 3000);
    };

    canvas.onclick = (e) => {
        if (!player.isPlaying) {
            player.play();
        }
    };

    player.notify = (msg) => {
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

    pauseBtn.addEventListener("click", function() {
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
    play_container.addEventListener("click", function() {
        player.play();
    });

    player.onStart = () => {
        pauseBtn.classList.add('pause');
        pauseBtn.classList.remove('play');
        play_container.classList.add('hidden');
    };

    player.onStop = () => {
        pauseBtn.classList.add('play');
        pauseBtn.classList.remove('pause');
        play_container.classList.remove('hidden');
        player.clients = [];
    };

    let update_details = () => {
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
    details.addEventListener('mouseleave', () => {
        details.classList.add('hidden');
    });

    const info_fps = document.getElementById('fps');
    const info_delay = document.getElementById('delay');
    const info_speed = document.getElementById('speed');
    const info_state = document.getElementById('state');
    const info_clients = document.getElementById('clients');
    const info_decoder = document.getElementById('decoder');
    const details_listener = () => {
        update_details();
        details.classList.remove('hidden');
    };
    info_clients.addEventListener('mouseenter', details_listener);
    info_state.addEventListener('mouseenter', details_listener);

    let clients = 0;

    setInterval(() => {
        let fps = player.getFPS();
        fps = player.isPlaying ? fps.toFixed(2) : '0';
        info_fps.textContent = `${fps} fps`;

        const delay = player.getDelay();
        info_delay.textContent = `${delay} ms`;

        const speed = player.getSpeed();
        info_speed.textContent = `${speed}`;

        const state = player.isPlaying ? '▶︎' : '🆇';
        info_state.textContent = `${state}`;

        if (clients != player.connections) {
            update_details();
        }
        clients = player.isPlaying ? player.connections : '?';
        info_clients.textContent = `${clients}`;

        const decoder = player.getDecoderName();
        info_decoder.textContent = decoder;

    }, 1000);

    player.play();

})();
