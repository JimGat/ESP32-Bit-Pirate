const jedecChips = new Map([
  ["ef4014", { manufacturer: "Winbond", model: "W25Q80", capacity: 1024 * 1024 }],
  ["ef4015", { manufacturer: "Winbond", model: "W25Q16", capacity: 2 * 1024 * 1024 }],
  ["ef4016", { manufacturer: "Winbond", model: "W25Q32", capacity: 4 * 1024 * 1024 }],
  ["ef4017", { manufacturer: "Winbond", model: "W25Q64", capacity: 8 * 1024 * 1024 }],
  ["ef4018", { manufacturer: "Winbond", model: "W25Q128", capacity: 16 * 1024 * 1024 }],
  ["ef4019", { manufacturer: "Winbond", model: "W25Q256", capacity: 32 * 1024 * 1024 }],
  ["c22015", { manufacturer: "Macronix", model: "MX25L1606E", capacity: 2 * 1024 * 1024 }],
  ["c22016", { manufacturer: "Macronix", model: "MX25L3206E", capacity: 4 * 1024 * 1024 }],
  ["c22017", { manufacturer: "Macronix", model: "MX25L6406E", capacity: 8 * 1024 * 1024 }],
  ["c22018", { manufacturer: "Macronix", model: "MX25L12835F", capacity: 16 * 1024 * 1024 }],
  ["c84016", { manufacturer: "GigaDevice", model: "GD25Q32", capacity: 4 * 1024 * 1024 }],
  ["c84017", { manufacturer: "GigaDevice", model: "GD25Q64", capacity: 8 * 1024 * 1024 }],
  ["c84018", { manufacturer: "GigaDevice", model: "GD25Q128", capacity: 16 * 1024 * 1024 }],
  ["1c3015", { manufacturer: "Eon", model: "EN25Q16", capacity: 2 * 1024 * 1024 }],
  ["1c3016", { manufacturer: "Eon", model: "EN25Q32", capacity: 4 * 1024 * 1024 }],
  ["1c3017", { manufacturer: "Eon", model: "EN25Q64", capacity: 8 * 1024 * 1024 }],
  ["204015", { manufacturer: "Micron", model: "N25Q016", capacity: 2 * 1024 * 1024 }],
  ["204016", { manufacturer: "Micron", model: "N25Q032", capacity: 4 * 1024 * 1024 }],
  ["204017", { manufacturer: "Micron", model: "N25Q064", capacity: 8 * 1024 * 1024 }],
  ["204018", { manufacturer: "Micron", model: "N25Q128", capacity: 16 * 1024 * 1024 }],
]);

export function identifyJedec(jedecId) {
  const key = Array.from(jedecId, (byte) => byte.toString(16).padStart(2, "0")).join("");
  return jedecChips.get(key) || null;
}
