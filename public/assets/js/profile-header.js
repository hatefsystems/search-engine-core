// A failed image reveals the underlying gradient or avatar placeholder.
document.querySelectorAll('[data-profile-image]').forEach((image) => {
    const revealPlaceholder = () => { image.hidden = true; };
    image.addEventListener('error', revealPlaceholder, { once: true });
    if (image.complete && image.naturalWidth === 0) revealPlaceholder();
});
