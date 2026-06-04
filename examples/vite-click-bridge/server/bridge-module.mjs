export async function doubleLater(value) {
  await new Promise((resolve) => setTimeout(resolve, 60));
  return value * 2;
}
