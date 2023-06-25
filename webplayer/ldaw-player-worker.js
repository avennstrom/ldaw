class Mutex {
    constructor() {
        let current = Promise.resolve();
        this.lock = () => {
            let _resolve;
            const p = new Promise(resolve => {
                _resolve = () => resolve();
            });
            // Caller gets a promise that resolves when the current outstanding
            // lock resolves
            const rv = current.then(() => _resolve);
            // Don't allow the next request until the new promise is done
            current = p;
            // Return the new promise
            return rv;
        };
    }
}

self.mutex = new Mutex();

self.onmessage = async e => {
    //console.log(e.data);
    const unlock = await self.mutex.lock();

    try {
        switch (e.data._) {
            case 'set-song':
                await onSetSongMessage(e.data);
                break;
            case 'render':
                await onRenderMessage(e.data);
                break;
        }
    } finally {
        unlock();
    }
};

const onSetSongMessage = async message => {
    const result = await WebAssembly.instantiate(message.song);
    self.song = result.instance.exports;
    self.stateMemory = 0;

    if (self.song.info) {
        const songInfoMemory = self.song.ldaw__alloc_(BigInt(16));
        const songInfoView = new DataView(self.song.memory.buffer, songInfoMemory, 16);

        songInfoView.setUint32(0, 44100, true);
        songInfoView.setUint32(4, 0, true);

        self.song.info(songInfoMemory);

        const sampleRate = songInfoView.getUint32(0, true);
        const stateSize = songInfoView.getUint32(4, true);

        //console.log({ sampleRate, stateSize });

        self.stateMemory = self.song.ldaw__alloc_(BigInt(stateSize));

        if (self.song.init) {
            self.song.init(self.stateMemory);
        }
    }

    self.sampleMemory = self.song.ldaw__alloc_(BigInt(message.bufferLength * 4));
};

const onRenderMessage = async message => {
    //console.log(`onRenderMessage(${message.id})`);
    self.song.play(self.sampleMemory, message.length, message.sampleRate, message.offset, self.stateMemory);
    const samples = new Float32Array(self.song.memory.buffer, self.sampleMemory, message.bufferLength);
    self.postMessage({ id: message.id, samples }, { samples });
    //console.log(`Song rendered (${message.id})`);
};