/**
 * P1 Meter DSMR – Custom scripts
 * - Infinite horizontal gallery
 * - Load modal content from separate HTML files
 */

(function () {
  "use strict";

  const track = document.getElementById("galleryTrack");
  if (!track) return;

  // Duplicate cards for seamless infinite scroll
  const cards = Array.from(track.children);
  cards.forEach((card) => {
    track.appendChild(card.cloneNode(true));
  });

  // Bootstrap modal
  const modalEl = document.getElementById("screenshotModal");
  const modalContent = document.getElementById("modalContent");
  if (!modalEl || !modalContent) return;

  const bsModal = new bootstrap.Modal(modalEl);

  // Cache loaded modal HTML so we don't re-fetch
  const cache = {};

  async function loadModal(url) {
    if (cache[url]) {
      return cache[url];
    }
    const response = await fetch(url);
    if (!response.ok) {
      throw new Error("Failed to load modal content");
    }
    const html = await response.text();
    cache[url] = html;
    return html;
  }

  track.addEventListener("click", async function (e) {
    const card = e.target.closest(".gallery-card");
    if (!card) return;

    const modalUrl = card.dataset.modal;
    if (!modalUrl) return;

    // Show loading state
    modalContent.innerHTML = `
      <div class="modal-header">
        <h5 class="modal-title">Loading…</h5>
        <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button>
      </div>
      <div class="modal-body text-center p-5">
        <div class="spinner-border text-light" role="status">
          <span class="visually-hidden">Loading…</span>
        </div>
      </div>
      <div class="modal-footer"></div>
    `;
    bsModal.show();

    try {
      const html = await loadModal(modalUrl);
      modalContent.innerHTML = html;
    } catch (err) {
      modalContent.innerHTML = `
        <div class="modal-header">
          <h5 class="modal-title">Error</h5>
          <button type="button" class="btn-close" data-bs-dismiss="modal" aria-label="Close"></button>
        </div>
        <div class="modal-body p-4">
          <p class="mb-0 text-muted">Could not load the screenshot details.</p>
        </div>
        <div class="modal-footer"></div>
      `;
      console.error(err);
    }
  });
})();
