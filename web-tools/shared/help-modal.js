const helpButton = document.querySelector("[data-help-open]");
const helpSource = document.getElementById("toolHelpContent") ?? document.getElementById("toolHelpTemplate");

if (helpButton && helpSource) {
  const dialog = document.createElement("div");
  dialog.className = "tool-help-overlay";
  dialog.hidden = true;
  dialog.innerHTML = `
    <section class="tool-help-modal" role="dialog" aria-modal="true" aria-labelledby="toolHelpTitle" tabindex="-1">
      <button class="tool-help-close" type="button" aria-label="Close help">x</button>
      <div class="tool-help-body"></div>
    </section>
  `;

  const modal = dialog.querySelector(".tool-help-modal");
  const body = dialog.querySelector(".tool-help-body");
  const closeButton = dialog.querySelector(".tool-help-close");
  let previousFocus = null;

  if ("content" in helpSource) {
    body.append(helpSource.content.cloneNode(true));
  } else {
    body.append(...helpSource.childNodes);
    helpSource.remove();
  }
  document.body.append(dialog);

  function openHelp() {
    previousFocus = document.activeElement;
    dialog.hidden = false;
    document.body.classList.add("tool-help-open");
    helpButton.setAttribute("aria-expanded", "true");
    modal.focus();
  }

  function closeHelp() {
    dialog.hidden = true;
    document.body.classList.remove("tool-help-open");
    helpButton.setAttribute("aria-expanded", "false");
    previousFocus?.focus?.();
  }

  helpButton.addEventListener("click", openHelp);
  closeButton.addEventListener("click", closeHelp);
  dialog.addEventListener("click", (event) => {
    if (event.target === dialog) closeHelp();
  });
  document.addEventListener("keydown", (event) => {
    if (!dialog.hidden && event.key === "Escape") closeHelp();
  });
}
