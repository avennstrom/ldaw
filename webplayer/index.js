import LdawPlayer from "./ldaw-player.js";

document.addEventListener('DOMContentLoaded', () => {
    const fileInput = document.getElementById('file-input');
    const volumeInput = document.getElementById('volume-input');

    const player = new LdawPlayer(volumeInput.value);

    fileInput.addEventListener('change', () => {
        player.playFile(fileInput.files[0]);
    });

    volumeInput.addEventListener('input', () => {
        player.setVolume(volumeInput.value);
    });
});