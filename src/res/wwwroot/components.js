export class StateSavedCheckbox extends HTMLInputElement {
    constructor() {
        super();

        const id = this.getAttribute("id");
        if (!id || id == "null") {
            throw "'id' property must be specified for Checkbox.";
        }

        this.setAttribute("type", "checkbox");

        const localStorageKey = `app-checkbox-${id}-state`;
        if (localStorage.getItem(localStorageKey) == "1") {
            this.checked = true;
        }

        const originalClientEvent = this.onclick;
        this.onclick = () => {
            if (this.checked) {
                localStorage.setItem(localStorageKey, "1");
            } else {
                localStorage.removeItem(localStorageKey);
            }
            if (originalClientEvent) originalClientEvent();
        }
    }
}

customElements.define("state-saved-checkbox", StateSavedCheckbox, { extends: "input" });