var links = document.querySelectorAll('link[rel="import"]')

for (i = 0; i < links.length ; i++) {
  let template = links[i].import.querySelector('.template');
  let clone = document.importNode(template.content, true);
  let parentId = links[i].dataset.parent;
  document.getElementById(parentId).appendChild(clone);
}
