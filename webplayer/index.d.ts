/**
 * LdawPlayer can play one song concurrently.
 */
export class LdawPlayer {
    /**
     * @param volume Volume from 0.0 to 1.0
     */
    constructor(volume: number);

    /**
     * Begin playback, load module from a buffer.
     * @param buffer The buffer that holds the WebAssembly.
     */
    public async playBuffer(buffer: ArrayBuffer): Promise<void>;

    /**
     * Begin playback, load module from a file.
     * @param file The file that contains the WebAssembly.
     */
    public async playFile(file: File): Promise<void>;

    /**
     * Begin playback, load module from a remote resource.
     * @param url The resource that contains the WebAssembly.
     */
    public async playUrl(url: string): Promise<void>;

    /**
     * Configure playback volume.
     * @param volume Volume from 0.0 to 1.0
     */
    public setVolume(volume: number): void;
}