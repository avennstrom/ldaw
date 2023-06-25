export default class LdawPlayer {
    /**
     * @param {number} volume 
     */
    constructor(volume) {
        this.worker = new Worker('ldaw-player-worker.js', { type: 'module' });
        this.worker.onmessage = this.receiveFromWorker.bind(this);

        this.sampleRate = 44100;
        this.bufferLength = this.sampleRate * 2;
        this.bufferCount = 3;

        this.baseSample = 0;

        this.audioContext = new AudioContext({
            sampleRate: this.sampleRate,
        });

        this.buffers = [];
        this.bufferNodes = [];
        for (let i = 0; i < this.bufferCount; ++i) {
            this.buffers.push(
                this.audioContext.createBuffer(1, this.bufferLength, this.sampleRate)
            );
            this.bufferNodes.push(undefined);
        }

        this.bufferIndex = 0;

        this.gainNode = this.audioContext.createGain();
        this.gainNode.connect(this.audioContext.destination);
        this.gainNode.gain.value = volume;

        this.isPlaying = false;
    }

    receiveFromWorker(e) {
        const message = e.data;
        const bufferIndex = message.id;

        const buffer = this.buffers[bufferIndex];
        buffer.copyToChannel(message.samples, 0);

        const node = this.audioContext.createBufferSource();
        node.connect(this.gainNode);
        node.buffer = buffer;
        node.channelCount = 1;
        node.onended = this.onBufferEnded.bind(this, bufferIndex);
        this.bufferNodes[bufferIndex] = node;

        if (!this.isPlaying) {
            this.bufferNodes[bufferIndex].start();
            this.isPlaying = true;
        }
    }

    onBufferEnded(bufferIndex) {
        this.bufferNodes[bufferIndex] = undefined;

        const nextBufferIndex = (this.bufferIndex + 1) % this.bufferCount;

        this.bufferNodes[nextBufferIndex].start();
        this.requestPlayback(bufferIndex, this.baseSample);

        this.bufferIndex = nextBufferIndex;
        this.baseSample += this.bufferLength;
    }

    requestPlayback(bufferIndex, baseSample) {
        this.worker.postMessage({
            _: 'render',
            id: bufferIndex,
            length: this.bufferLength,
            offset: BigInt(baseSample),
            sampleRate: BigInt(this.sampleRate),
        });
    }

    /**
     * @param {ArrayBuffer} buffer 
     */
    async playBuffer(buffer) {
        this.worker.postMessage({
            _: 'set-song',
            song: buffer,
            bufferLength: this.bufferLength,
        });

        this.baseSample = 0;
        for (let i = 0; i < this.bufferCount; ++i) {
            this.requestPlayback(i, this.baseSample + (this.bufferLength * i));
        }

        this.baseSample += (this.bufferLength * this.bufferCount);
    }

    /**
     * @param {File} file 
     */
    async playFile(file) {
        const buffer = await file.arrayBuffer();

        this.playBuffer(buffer);
    }

    /**
     * @param {string} url 
     */
    async playUrl(url) {
        const result = await fetch(url)
        const buffer = await result.arrayBuffer();

        this.playBuffer(buffer);
    }

    /**
     * @param {number} volume 
     */
    setVolume(volume) {
        this.gainNode.gain.value = volume;
    }
}