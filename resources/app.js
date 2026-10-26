(function () {
    const $ = (selector, root) => (root || document).querySelector(selector);
    const $$ = (selector, root) => Array.from((root || document).querySelectorAll(selector));
    const body = document.body;

    function poll(url, interval, handle) {
        let timer = 0;
        const tick = () => fetch(url, { cache: 'no-store' })
            .then(response => response.text())
            .then(text => { if (handle(text) === false) { clearInterval(timer); } })
            .catch(() => {});
        tick();
        timer = setInterval(tick, interval);
    }

    if (body.dataset.status) {
        poll(body.dataset.status, 3000, text => {
            const status = JSON.parse(text);
            for (const key of Object.keys(status)) {
                const el = document.getElementById(key);
                if (el) {
                    (el.querySelector('a') || el).textContent = status[key];
                }
            }
            return status.stop != 1;
        });
    }

    if (body.dataset.duo) {
        poll(body.dataset.duo, 2000, text => {
            if (text !== '1' && text !== '0') {
                return true;
            }
            const result = $('#duoresult');
            const label = body.dataset.duoLabel;
            if (text === '1') {
                result.textContent = label + ' approved, redirecting...';
                setTimeout(() => {
                    const type = body.dataset.duoType;
                    if (type === '1') {
                        window.open(body.dataset.duoOpen);
                    }
                    if (type === '0' || type === '1') {
                        location.href = '/';
                    } else {
                        $('#frmrepost').submit();
                    }
                }, 2000);
            } else {
                result.textContent = label + ' failed, refresh to retry';
            }
            return false;
        });
    }

    const aplist = $('#aplist');
    if (aplist) {
        poll('/ssidlist', 3000, html => { aplist.innerHTML = html; });
    }

    const hassCheckbox = $('input[type=checkbox].chkHass');
    const hassTopic = $('input[type=text].chkHass');
    if (hassCheckbox && hassTopic) {
        hassCheckbox.addEventListener('change', () => {
            if (hassCheckbox.checked) {
                if (!hassTopic.value) {
                    hassTopic.value = 'homeassistant';
                }
            } else {
                hassTopic.value = '';
            }
        });
        hassTopic.addEventListener('input', () => { hassCheckbox.checked = hassTopic.value.length > 0; });
    }

    const phy = $('select[name=NWCUSTPHY]');
    if (phy) {
        const show = (className, visible) => $$('.' + className).forEach(el => { (el.closest('tr') || el).hidden = !visible; });
        const apply = () => {
            const value = Number(phy.value);
            show('internalopt', value >= 4 && value <= 9);
            show('externalopt', value >= 1 && value <= 3);
        };
        phy.addEventListener('change', apply);
        apply();
    }

    const credentials = $('#credfrm');
    if (credentials) {
        credentials.addEventListener('submit', event => {
            const user = $('#inputuser').value;
            const pass = $('#inputpass').value;
            const pass2 = $('#inputpass2').value;
            if (user === '#' || user === '') {
                return;
            }
            const printable = /^[ -~]*$/;
            let message = '';
            if (pass !== pass2) {
                message = 'Passwords do not match';
            } else if (!printable.test(user) || !printable.test(pass)) {
                message = 'Only non unicode characters are allowed in username and password';
            }
            if (message) {
                alert(message);
                event.preventDefault();
            }
        });
    }

    $$('form[data-confirm]').forEach(form => form.addEventListener('submit', event => {
        const compare = form.dataset.sameAs;
        if (compare && form.dataset.sameBuild === '1' && $('#currentver').textContent === $('#' + compare).textContent) {
            alert('You are already on this version, build and build type');
            event.preventDefault();
            return;
        }
        if (!confirm(form.dataset.confirm)) {
            event.preventDefault();
        }
    }));

    const upload = $('#submitbtn');
    if (upload) {
        upload.addEventListener('click', () => {
            ['autoupdform', 'rebootform', 'upform', 'gitdiv'].forEach(id => {
                const el = document.getElementById(id);
                if (el) {
                    el.hidden = true;
                }
            });
            $('#msgdiv').hidden = false;
        });
    }

    $$('input[type=file][data-fill]').forEach(input => input.addEventListener('change', () => {
        if (!input.files.length) {
            return;
        }
        const reader = new FileReader();
        reader.onload = event => { $(input.dataset.fill).value = event.target.result; };
        reader.readAsText(input.files[0]);
    }));

    const gpioOptions = $('#gpiooptions');
    if (gpioOptions) {
        $$('select.gpioselect').forEach(select => {
            select.innerHTML = gpioOptions.innerHTML;
            select.value = select.dataset.value;
            if (select.value == 21) {
                select.disabled = true;
            }
        });
    }

    function copyRedacted(source) {
        const clone = source.cloneNode(true);
        $$('.redact', clone).forEach(el => { el.textContent = '***'; });
        const text = clone.textContent;
        if (navigator.clipboard && window.isSecureContext) {
            navigator.clipboard.writeText(text);
            return;
        }
        const area = document.createElement('textarea');
        area.value = text;
        area.style.position = 'fixed';
        area.style.left = '-9999px';
        document.body.append(area);
        area.select();
        try {
            document.execCommand('copy');
        } finally {
            area.remove();
        }
    }

    document.addEventListener('click', event => {
        const ssid = event.target.closest('tr[data-ssid]');
        if (ssid) {
            $('#inputssid').value = ssid.dataset.ssid;
        }

        const generate = event.target.closest('[data-gen]');
        if (generate) {
            const field = document.getElementsByName(generate.dataset.gen)[0];
            field.type = 'text';
            field.value = generate.dataset.value;
            generate.closest('tr').hidden = true;
        }

        const check = event.target.closest('[data-check]');
        if (check) {
            $$('input[type=checkbox].' + check.dataset.check).forEach(box => { box.checked = check.dataset.value === '1'; });
        }

        const toggle = event.target.closest('[data-toggle]');
        if (toggle) {
            event.preventDefault();
            const target = $(toggle.dataset.toggle);
            target.hidden = !target.hidden;
        }

        const copy = event.target.closest('[data-copy]');
        if (copy) {
            copyRedacted($(copy.dataset.copy));
        }

        const redirect = event.target.closest('[data-redirect-after]');
        if (redirect) {
            setTimeout(() => { location.href = redirect.dataset.redirectAfter; }, 1000);
        }
    });
})();
