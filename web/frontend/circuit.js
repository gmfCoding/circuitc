/*
 * CircuitC Viewer - JavaScript WebSocket Client
 * Binary protocol implementation
 */

// Element types (must match circuit.h)
const ElementType = {
    ELEM_RESISTOR: 0,
    ELEM_CAPACITOR: 1,
    ELEM_INDUCTOR: 2,
    ELEM_VOLTAGE_SOURCE: 3,
    ELEM_CURRENT_SOURCE: 4,
    ELEM_DIODE: 5,
    ELEM_TRANSISTOR_NPN: 6,
    ELEM_TRANSISTOR_PNP: 7,
    ELEM_SWITCH: 8
};

// Message types
const MSG_CIRCUIT_DATA = 0x01;
const MSG_SIM_UPDATE = 0x02;
const MSG_CONTROL = 0x03;
const MSG_LOAD_FILE = 0x04;
const MSG_ERROR = 0xFF;

// Control commands
const CMD_START = 0x01;
const CMD_STOP = 0x02;
const CMD_STEP = 0x03;
const CMD_RESET = 0x04;
const CMD_SET_SPEED = 0x05;
const CMD_SET_CURRENT_VIS = 0x06;

class CircuitViewer {
    constructor() {
        this.ws = null;
        this.circuit = null;
        this.canvas = document.getElementById('circuit-canvas');
        this.ctx = this.canvas.getContext('2d');
        
        // Display settings
        this.gridSize = 16;
        this.offsetX = 50;
        this.offsetY = 50;
        this.scale = 1.0;
        
        this.initUI();
        this.connect();
        this.resizeCanvas();
        
        window.addEventListener('resize', () => this.resizeCanvas());
    }
    
    initUI() {
        // Connection status
        document.getElementById('load-btn').addEventListener('click', () => this.loadCircuit());
        document.getElementById('start-btn').addEventListener('click', () => this.sendControl(CMD_START));
        document.getElementById('stop-btn').addEventListener('click', () => this.sendControl(CMD_STOP));
        document.getElementById('step-btn').addEventListener('click', () => this.sendControl(CMD_STEP));
        document.getElementById('reset-btn').addEventListener('click', () => this.sendControl(CMD_RESET));
        
        // Speed control
        const speedSlider = document.getElementById('speed-slider');
        speedSlider.addEventListener('input', (e) => {
            const value = parseFloat(e.target.value);
            document.getElementById('speed-value').textContent = value.toFixed(1) + 'x';
            this.sendControlWithFloat(CMD_SET_SPEED, value);
        });
        
        // Current visualization control
        const currentSlider = document.getElementById('current-slider');
        currentSlider.addEventListener('input', (e) => {
            const value = parseFloat(e.target.value);
            document.getElementById('current-value').textContent = value.toFixed(1) + 'x';
            this.sendControlWithFloat(CMD_SET_CURRENT_VIS, value);
        });
    }
    
    connect() {
        // Determine WebSocket URL based on current page location
        const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
        const host = window.location.host || 'localhost:8080';
        
        // Try multiple WebSocket URLs in order
        const wsUrls = [
            `${protocol}//${host}`,  // Same host as the page
            'ws://localhost:8080',
            'ws://127.0.0.1:8080',
            'wss://fantastic-bassoon-q76qrp94prx3xgxg-9000.app.github.dev',
            'ws://localhost:9000',
            'ws://127.0.0.1:9000'
        ];
        
        this.tryConnect(wsUrls, 0);
    }
    
    tryConnect(urls, index) {
        if (index >= urls.length) {
            console.error('All WebSocket connection attempts failed');
            document.getElementById('connection-status').textContent = 'Connection Failed';
            document.getElementById('connection-status').className = 'status-indicator disconnected';
            // Retry from beginning after 5 seconds
            setTimeout(() => this.connect(), 5000);
            return;
        }
        
        const url = urls[index];
        console.log(`Attempting WebSocket connection to: ${url}`);
        
        this.ws = new WebSocket(url);
        this.ws.binaryType = 'arraybuffer';
        
        const connectTimeout = setTimeout(() => {
            console.log(`Connection timeout for ${url}, trying next...`);
            this.ws.close();
            this.tryConnect(urls, index + 1);
        }, 3000);
        
        this.ws.onopen = () => {
            clearTimeout(connectTimeout);
            console.log(`WebSocket connected to: ${url}`);
            this.connectedUrl = url;
            document.getElementById('connection-status').textContent = 'Connected';
            document.getElementById('connection-status').className = 'status-indicator connected';
        };
        
        this.ws.onclose = () => {
            clearTimeout(connectTimeout);
            console.log('WebSocket disconnected');
            document.getElementById('connection-status').textContent = 'Disconnected';
            document.getElementById('connection-status').className = 'status-indicator disconnected';
            
            // If we were connected before, try to reconnect to same URL
            if (this.connectedUrl) {
                setTimeout(() => {
                    this.ws = new WebSocket(this.connectedUrl);
                    this.setupWebSocketHandlers();
                }, 2000);
            } else {
                // Try next URL
                this.tryConnect(urls, index + 1);
            }
        };
        
        this.ws.onerror = (error) => {
            clearTimeout(connectTimeout);
            console.error(`WebSocket error for ${url}:`, error);
        };
        
        this.ws.onmessage = (event) => {
            this.handleMessage(new Uint8Array(event.data));
        };
    }
    
    setupWebSocketHandlers() {
        if (!this.ws) return;
        
        this.ws.binaryType = 'arraybuffer';
        
        this.ws.onopen = () => {
            console.log(`WebSocket reconnected to: ${this.connectedUrl}`);
            document.getElementById('connection-status').textContent = 'Connected';
            document.getElementById('connection-status').className = 'status-indicator connected';
        };
        
        this.ws.onclose = () => {
            console.log('WebSocket disconnected');
            document.getElementById('connection-status').textContent = 'Disconnected';
            document.getElementById('connection-status').className = 'status-indicator disconnected';
            
            // Try to reconnect
            if (this.connectedUrl) {
                setTimeout(() => {
                    this.ws = new WebSocket(this.connectedUrl);
                    this.setupWebSocketHandlers();
                }, 2000);
            }
        };
        
        this.ws.onerror = (error) => {
            console.error('WebSocket error:', error);
        };
        
        this.ws.onmessage = (event) => {
            this.handleMessage(new Uint8Array(event.data));
        };
    }
    
    handleMessage(data) {
        if (data.length < 1) return;
        
        const msgType = data[0];
        
        switch (msgType) {
            case MSG_CIRCUIT_DATA:
                this.handleCircuitData(data);
                break;
            case MSG_SIM_UPDATE:
                this.handleSimUpdate(data);
                break;
            case MSG_ERROR:
                const errorCode = data[1];
                console.error('Server error:', errorCode);
                alert('Error loading circuit or running simulation');
                break;
        }
    }
    
    handleCircuitData(data) {
        let offset = 1;
        
        // Read counts
        const elementCount = this.unpackUint16(data, offset);
        offset += 2;
        const nodeCount = this.unpackUint16(data, offset);
        offset += 2;
        
        console.log(`Circuit: ${elementCount} elements, ${nodeCount} nodes`);
        
        // Update UI
        document.getElementById('element-count').textContent = elementCount;
        document.getElementById('node-count').textContent = nodeCount;
        
        // Read elements
        const elements = [];
        for (let i = 0; i < elementCount; i++) {
            const elem = {
                type: data[offset++],
                nodes: [
                    this.unpackUint16(data, offset),
                    this.unpackUint16(data, offset + 2),
                    this.unpackUint16(data, offset + 4)
                ],
                value: this.unpackDouble(data, offset + 6),
                current: this.unpackDouble(data, offset + 14),
                volts: [
                    this.unpackDouble(data, offset + 22),
                    this.unpackDouble(data, offset + 30),
                    this.unpackDouble(data, offset + 38)
                ]
            };
            offset += 46;
            elements.push(elem);
        }
        
        this.circuit = {
            elementCount,
            nodeCount,
            elements,
            time: 0,
            stepCount: 0
        };
        
        this.draw();
    }
    
    handleSimUpdate(data) {
        if (!this.circuit) return;
        
        let offset = 1;
        
        // Read time and step count
        this.circuit.time = this.unpackDouble(data, offset);
        offset += 8;
        this.circuit.stepCount = this.unpackUint32(data, offset);
        offset += 4;
        
        // Update UI
        document.getElementById('sim-time').textContent = `Time: ${this.circuit.time.toFixed(3)}s`;
        document.getElementById('sim-steps').textContent = `Steps: ${this.circuit.stepCount}`;
        
        // Update element data
        for (let i = 0; i < this.circuit.elementCount; i++) {
            this.circuit.elements[i].current = this.unpackDouble(data, offset);
            offset += 8;
            this.circuit.elements[i].volts[0] = this.unpackDouble(data, offset);
            offset += 8;
            this.circuit.elements[i].volts[1] = this.unpackDouble(data, offset);
            offset += 8;
            this.circuit.elements[i].volts[2] = this.unpackDouble(data, offset);
            offset += 8;
        }
        
        this.draw();
    }
    
    loadCircuit() {
        const filename = document.getElementById('circuit-file').value;
        if (!filename) return;
        
        // Encode: MSG_LOAD_FILE + filename
        const encoder = new TextEncoder();
        const filenameBytes = encoder.encode(filename);
        const buffer = new Uint8Array(1 + filenameBytes.length);
        buffer[0] = MSG_LOAD_FILE;
        buffer.set(filenameBytes, 1);
        
        this.ws.send(buffer);
    }
    
    sendControl(command) {
        const buffer = new Uint8Array(2);
        buffer[0] = MSG_CONTROL;
        buffer[1] = command;
        this.ws.send(buffer);
    }
    
    sendControlWithFloat(command, value) {
        const buffer = new Uint8Array(6);
        buffer[0] = MSG_CONTROL;
        buffer[1] = command;
        this.packFloat(buffer, 2, value);
        this.ws.send(buffer);
    }
    
    // Binary unpacking (big-endian)
    unpackUint16(buffer, offset) {
        return (buffer[offset] << 8) | buffer[offset + 1];
    }
    
    unpackUint32(buffer, offset) {
        return (buffer[offset] << 24) | (buffer[offset + 1] << 16) | 
               (buffer[offset + 2] << 8) | buffer[offset + 3];
    }
    
    unpackDouble(buffer, offset) {
        const dataView = new DataView(buffer.buffer, offset, 8);
        return dataView.getFloat64(0, false); // big-endian
    }
    
    packFloat(buffer, offset, value) {
        const dataView = new DataView(buffer.buffer, offset, 4);
        dataView.setFloat32(0, value, false); // big-endian
    }
    
    resizeCanvas() {
        const container = this.canvas.parentElement;
        this.canvas.width = container.clientWidth - 40;
        this.canvas.height = container.clientHeight - 40;
        this.draw();
    }
    
    draw() {
        if (!this.circuit) {
            this.ctx.fillStyle = '#1a1a1a';
            this.ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
            
            this.ctx.fillStyle = '#666';
            this.ctx.font = '16px sans-serif';
            this.ctx.textAlign = 'center';
            this.ctx.fillText('Load a circuit to begin', this.canvas.width / 2, this.canvas.height / 2);
            return;
        }
        
        const ctx = this.ctx;
        
        // Clear
        ctx.fillStyle = '#1a1a1a';
        ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);
        
        // Draw grid
        this.drawGrid();
        
        // Draw elements
        for (let i = 0; i < this.circuit.elements.length; i++) {
            this.drawElement(this.circuit.elements[i], i);
        }
    }
    
    drawGrid() {
        const ctx = this.ctx;
        ctx.strokeStyle = '#333';
        ctx.lineWidth = 0.5;
        
        // Vertical lines
        for (let x = 0; x < this.canvas.width; x += this.gridSize) {
            ctx.beginPath();
            ctx.moveTo(x, 0);
            ctx.lineTo(x, this.canvas.height);
            ctx.stroke();
        }
        
        // Horizontal lines
        for (let y = 0; y < this.canvas.height; y += this.gridSize) {
            ctx.beginPath();
            ctx.moveTo(0, y);
            ctx.lineTo(this.canvas.width, y);
            ctx.stroke();
        }
    }
    
    drawElement(elem, index) {
        const ctx = this.ctx;
        
        // Calculate position based on element index (simple layout)
        const x = this.offsetX + (index % 5) * 120;
        const y = this.offsetY + Math.floor(index / 5) * 100;
        
        // Get voltage color
        const v1 = elem.volts[0];
        const v2 = elem.volts[1];
        const avgVolt = (v1 + v2) / 2;
        
        const voltColor = this.getVoltageColor(avgVolt);
        
        // Draw wire with current thickness
        const currentVis = parseFloat(document.getElementById('current-slider').value);
        const lineWidth = Math.max(1, Math.min(10, Math.abs(elem.current) * currentVis * 2));
        
        ctx.strokeStyle = voltColor;
        ctx.lineWidth = lineWidth;
        
        // Draw element symbol
        switch (elem.type) {
            case ElementType.ELEM_RESISTOR:
                this.drawResistor(ctx, x, y, elem);
                break;
            case ElementType.ELEM_CAPACITOR:
                this.drawCapacitor(ctx, x, y, elem);
                break;
            case ElementType.ELEM_INDUCTOR:
                this.drawInductor(ctx, x, y, elem);
                break;
            case ElementType.ELEM_VOLTAGE_SOURCE:
                this.drawVoltageSource(ctx, x, y, elem);
                break;
            case ElementType.ELEM_CURRENT_SOURCE:
                this.drawCurrentSource(ctx, x, y, elem);
                break;
            case ElementType.ELEM_DIODE:
                this.drawDiode(ctx, x, y, elem);
                break;
            case ElementType.ELEM_TRANSISTOR_NPN:
            case ElementType.ELEM_TRANSISTOR_PNP:
                this.drawTransistor(ctx, x, y, elem);
                break;
            case ElementType.ELEM_SWITCH:
                this.drawSwitch(ctx, x, y, elem);
                break;
        }
        
        // Draw labels
        ctx.fillStyle = '#ccc';
        ctx.font = '10px monospace';
        ctx.textAlign = 'center';
        ctx.fillText(this.getElementName(elem.type), x + 30, y - 10);
        ctx.fillText(`${elem.current.toExponential(2)}A`, x + 30, y + 50);
        ctx.fillText(`${v1.toFixed(2)}V / ${v2.toFixed(2)}V`, x + 30, y + 62);
    }
    
    drawResistor(ctx, x, y, elem) {
        ctx.beginPath();
        ctx.moveTo(x, y + 20);
        ctx.lineTo(x + 10, y + 20);
        // Zigzag
        ctx.lineTo(x + 15, y + 10);
        ctx.lineTo(x + 20, y + 30);
        ctx.lineTo(x + 25, y + 10);
        ctx.lineTo(x + 30, y + 30);
        ctx.lineTo(x + 35, y + 10);
        ctx.lineTo(x + 40, y + 30);
        ctx.lineTo(x + 45, y + 10);
        ctx.lineTo(x + 50, y + 20);
        ctx.lineTo(x + 60, y + 20);
        ctx.stroke();
    }
    
    drawCapacitor(ctx, x, y, elem) {
        ctx.beginPath();
        ctx.moveTo(x, y + 20);
        ctx.lineTo(x + 25, y + 20);
        ctx.moveTo(x + 25, y + 5);
        ctx.lineTo(x + 25, y + 35);
        ctx.moveTo(x + 35, y + 5);
        ctx.lineTo(x + 35, y + 35);
        ctx.moveTo(x + 35, y + 20);
        ctx.lineTo(x + 60, y + 20);
        ctx.stroke();
    }
    
    drawInductor(ctx, x, y, elem) {
        ctx.beginPath();
        ctx.moveTo(x, y + 20);
        ctx.lineTo(x + 10, y + 20);
        // Coils
        for (let i = 0; i < 4; i++) {
            ctx.arc(x + 15 + i * 10, y + 20, 5, Math.PI, 0, false);
        }
        ctx.lineTo(x + 60, y + 20);
        ctx.stroke();
    }
    
    drawVoltageSource(ctx, x, y, elem) {
        ctx.beginPath();
        ctx.arc(x + 30, y + 20, 15, 0, Math.PI * 2);
        ctx.stroke();
        
        ctx.fillStyle = '#ccc';
        ctx.font = '16px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText('V', x + 30, y + 25);
    }
    
    drawCurrentSource(ctx, x, y, elem) {
        ctx.beginPath();
        ctx.arc(x + 30, y + 20, 15, 0, Math.PI * 2);
        ctx.stroke();
        
        ctx.fillStyle = '#ccc';
        ctx.font = '16px sans-serif';
        ctx.textAlign = 'center';
        ctx.fillText('I', x + 30, y + 25);
    }
    
    drawDiode(ctx, x, y, elem) {
        ctx.beginPath();
        ctx.moveTo(x, y + 20);
        ctx.lineTo(x + 20, y + 20);
        // Triangle
        ctx.moveTo(x + 20, y + 5);
        ctx.lineTo(x + 20, y + 35);
        ctx.lineTo(x + 40, y + 20);
        ctx.closePath();
        ctx.stroke();
        // Cathode line
        ctx.moveTo(x + 40, y + 5);
        ctx.lineTo(x + 40, y + 35);
        ctx.moveTo(x + 40, y + 20);
        ctx.lineTo(x + 60, y + 20);
        ctx.stroke();
    }
    
    drawTransistor(ctx, x, y, elem) {
        ctx.beginPath();
        // Base line
        ctx.moveTo(x, y + 20);
        ctx.lineTo(x + 20, y + 20);
        // Vertical line
        ctx.moveTo(x + 20, y + 5);
        ctx.lineTo(x + 20, y + 35);
        // Collector
        ctx.moveTo(x + 20, y + 10);
        ctx.lineTo(x + 40, y + 5);
        // Emitter
        ctx.moveTo(x + 20, y + 30);
        ctx.lineTo(x + 40, y + 35);
        ctx.stroke();
        
        // Arrow (NPN down, PNP up)
        ctx.beginPath();
        if (elem.type === ElementType.ELEM_TRANSISTOR_NPN) {
            ctx.moveTo(x + 35, y + 28);
            ctx.lineTo(x + 40, y + 35);
            ctx.lineTo(x + 33, y + 33);
        } else {
            ctx.moveTo(x + 35, y + 12);
            ctx.lineTo(x + 40, y + 5);
            ctx.lineTo(x + 33, y + 7);
        }
        ctx.stroke();
    }
    
    drawSwitch(ctx, x, y, elem) {
        ctx.beginPath();
        ctx.moveTo(x, y + 20);
        ctx.lineTo(x + 20, y + 20);
        // Switch contact
        ctx.moveTo(x + 20, y + 20);
        ctx.lineTo(x + 40, y + 10);  // Open position shown
        ctx.moveTo(x + 40, y + 20);
        ctx.lineTo(x + 60, y + 20);
        ctx.stroke();
    }
    
    getVoltageColor(voltage) {
        if (voltage > 3) return '#ff4444';
        if (voltage > 1) return '#ffaa00';
        if (voltage > -1) return '#44ff44';
        if (voltage > -3) return '#4444ff';
        return '#8844ff';
    }
    
    getElementName(type) {
        const names = {
            [ElementType.ELEM_RESISTOR]: 'Resistor',
            [ElementType.ELEM_CAPACITOR]: 'Capacitor',
            [ElementType.ELEM_INDUCTOR]: 'Inductor',
            [ElementType.ELEM_VOLTAGE_SOURCE]: 'Voltage',
            [ElementType.ELEM_CURRENT_SOURCE]: 'Current',
            [ElementType.ELEM_DIODE]: 'Diode',
            [ElementType.ELEM_TRANSISTOR_NPN]: 'NPN',
            [ElementType.ELEM_TRANSISTOR_PNP]: 'PNP',
            [ElementType.ELEM_SWITCH]: 'Switch'
        };
        return names[type] || 'Unknown';
    }
}

// Initialize on load
window.addEventListener('DOMContentLoaded', () => {
    new CircuitViewer();
});
