"use strict";

const installStatus = document.getElementById("install-status");
fetch("variants.json", {cache: "no-store"}).then(response => {
  if (!response.ok) throw new Error("Cannot load firmware options");
  return response.json();
}).then(({version, variants}) => {
  const available = ["nextion", "headless"].filter(variant => variants.includes(variant));
  for (const variant of available) document.getElementById(variant).hidden = false;
  for (const label of document.querySelectorAll(".v")) label.textContent = version ?? "—";
  installStatus.hidden = available.length > 0;
  installStatus.textContent = "No USB images available. / Нет образов для установки по USB.";
}).catch(() => {
  installStatus.textContent = "Could not load firmware options. / Не удалось загрузить варианты прошивки.";
});
