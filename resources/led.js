// The rule editor of the Status LED page. Served only with that page, and not in the updater.
// The page loads it before app.js, so that it is listening when the first status poll answers.
(function () {
    const $ = (selector, root) => (root || document).querySelector(selector);
    const $$ = (selector, root) => Array.from((root || document).querySelectorAll(selector));

    const ledRules = $('#ledrules');
    if (ledRules) {
        const form = ledRules.closest('form');
        const fields = form.elements;
        const value = $('#ledrulesvalue');
        const addButton = $('#ledadd');
        const countLabel = $('#ledcount');
        const saveBar = $('#ledsave');
        const saveButton = $('input[type=submit]', saveBar);
        const unsavedLabel = $('#ledunsaved');
        const discardButton = $('#leddiscard');
        const staleNote = $('#ledstale');
        const checkButton = $('#ledcheck');
        const checkRow = $('#ledcheckrow');
        const checkMessage = $('#ledcheckmsg');
        const shownDot = $('#ledshowndot');
        const shownColor = $('#ledshowncolor');
        const shownWhy = $('#ledshownwhy');
        const hintBlock = $('.ledhint', ledRules);
        const triggerTemplate = $('#ledtriggers');
        const effectTemplate = $('#ledeffects');
        const max = Number(ledRules.dataset.max) || 16;
        const canTest = ledRules.dataset.test === '1';
        const triggerIds = new Set($$('option', triggerTemplate.content).map(option => option.value));
        const eventTriggers = new Set($$('option[data-event]', triggerTemplate.content).map(option => option.value));
        const disabledTriggers = new Set($$('option[data-off]', triggerTemplate.content).map(option => option.value));
        const effectIds = new Set($$('option', effectTemplate.content).map(option => option.value));
        const timings = [
            ['while', 'while it applies'],
            ['after', 'after'],
            ['for', 'for the first']
        ];
        const checkSteps = [['ff0000', 'red', 1500], ['00ff00', 'green', 1500], ['0000ff', 'blue', 3000]];
        const momentNote = 'This is a moment, so it only works with "for the first".';
        const blackNote = 'This color is too dark for the LED to show.';
        // States that can last for hours, and the problems and moments that wait behind them
        const lastingTriggers = new Set(['locked', 'unlocked', 'blocked', 'dooropen', 'lockbattery', 'accbattery', 'rto', 'cm', 'openerbattery']);
        const noticeTriggers = new Set(['ap', 'netdown', 'unpaired', 'mqttdown', 'noresponse', 'motor', 'blocked', 'dooropen', 'lockbattery', 'accbattery', 'opening', 'ring', 'openerbattery']);
        // A lock, and an opener, is in only one of these states at a time. An open door is nearly never locked, and Nuki
        // Hub only counts a lost MQTT connection while the network is there.
        const exclusiveStates = [['locked', 'unlocked', 'motor', 'blocked'], ['rto', 'opening'], ['locked', 'dooropen'], ['netdown', 'mqttdown']];
        const exclusive = (a, b) => exclusiveStates.some(group => group.includes(a) && group.includes(b));
        // While a rule applies, the ones below it wait. This is worth saying where it can surprise: a problem or moment
        // below a lasting state, and everything below a rule that keeps the LED dark. That one normal state goes before
        // another is what the order is for, and a dark LED during an update or a start is over in moments.
        const briefTriggers = new Set(['update', 'starting']);
        const hides = (upper, lower) => upper.trigger !== lower.trigger && !exclusive(upper.trigger, lower.trigger) && !briefTriggers.has(upper.trigger)
            && (upper.effect === 'off' || (upper.timing !== 'for' && lastingTriggers.has(upper.trigger) && noticeTriggers.has(lower.trigger)));
        // The costly case of this, marked in the list: a normal state, which can last for hours, keeps a problem or a
        // moment from showing
        const normalStates = new Set(['locked', 'unlocked', 'dooropen', 'rto', 'cm']);
        const holdsBack = (upper, lower) => normalStates.has(upper.trigger) && upper.timing !== 'for' && noticeTriggers.has(lower.trigger) && hides(upper, lower);
        // States that are over in seconds, so that a Dark rule on them keeps the rules below waiting only that long
        const shortTriggers = new Set(['ble', 'motor', 'opening']);
        // A rule that is over before a rule above it ends, so that it is missed while that one applies, instead of waiting
        const fleeting = rule => rule.timing === 'for' || shortTriggers.has(rule.trigger);
        // The colors an LED is usually set to, offered by the color field and used for a new rule
        const palette = ['ff0000', 'ff6400', 'ffcc00', '00ff00', '00ffff', '0000ff', 'ff00ff', 'ffffff'];
        const colorNames = [[15, 'red'], [40, 'orange'], [70, 'yellow'], [160, 'green'], [200, 'cyan'], [255, 'blue'], [285, 'purple'], [325, 'magenta'], [345, 'pink'], [360, 'red']];
        // In words, because a dot says nothing to someone who can't tell the colors apart or can't see it
        const colorName = color => {
            const [r, g, b] = [0, 2, 4].map(start => parseInt(color.slice(start, start + 2), 16));
            const high = Math.max(r, g, b);
            const range = high - Math.min(r, g, b);
            if (high < 25) {
                return 'black';
            }
            if (range / high < .25) {
                return 'white';
            }
            const hue = ((high === r ? (g - b) / range : high === g ? (b - r) / range + 2 : (r - g) / range + 4) + 6) % 6 * 60;
            return colorNames.find(([limit]) => hue < limit)[1];
        };

        const element = (tag, className, text) => {
            const el = document.createElement(tag);
            if (className) {
                el.className = className;
            }
            if (text) {
                el.textContent = text;
            }
            return el;
        };
        const name = (el, text) => {
            el.setAttribute('aria-label', text);
            el.title = text;
        };
        const button = (className, text, label) => {
            const el = element('button', className, text);
            el.type = 'button';
            if (label) {
                name(el, label);
            }
            return el;
        };
        const select = (className, html, selected, label) => {
            const el = element('select', className);
            el.innerHTML = html;
            el.value = selected;
            el.setAttribute('aria-label', label);
            return el;
        };
        const colorInput = (color, label) => {
            const el = element('input', 'ledcolor');
            el.type = 'color';
            el.value = '#' + color;
            el.setAttribute('list', 'ledpalette');
            el.setAttribute('aria-label', label);
            return el;
        };
        // Ids, so that a control can point at the text that describes it
        let idCount = 0;
        const newId = () => 'ledtext' + ++idCount;
        const noteElement = () => {
            const el = element('span', 'lednote');
            el.hidden = true;
            // For the text of the note, which is made anew when it changes: up to its button, and after it
            el.dataset.textId = newId();
            el.dataset.restId = newId();
            return el;
        };
        const noteIds = note => note.dataset.textId + ' ' + note.dataset.restId;
        // Says more about what is chosen in a field of the open rule
        const hintElement = () => {
            const el = element('span', 'ledfieldhint');
            el.id = newId();
            el.hidden = true;
            return el;
        };
        const setHint = (hint, text) => {
            if (hint.textContent !== text) {
                hint.textContent = text;
            }
            hint.hidden = !text;
        };
        // A value that isn't taken: the field and the hint that says what to do look wrong
        const markWrong = (control, hint, wrong) => {
            if (wrong) {
                control.setAttribute('aria-invalid', 'true');
            } else {
                control.removeAttribute('aria-invalid');
            }
            hint.classList.toggle('ledwrong', wrong);
        };
        // A control of an open rule with its caption, and a hint under it. The control has its own name, with the rule number.
        const field = (tag, area, caption, control, hint) => {
            const el = element(tag, 'ledfield ledfield' + area);
            const text = element('span', 'ledcaption', caption);
            text.setAttribute('aria-hidden', 'true');
            el.append(text, control);
            if (hint) {
                el.append(hint);
            }
            return el;
        };
        // Show, with a dot that plays the rhythm of the effect, because the LED itself may be out of sight, and after it
        // whether the LED plays it right now. On the line of the caption, so that nothing moves when that ends.
        const showField = (control, hint) => {
            const el = field('label', 'show', 'Show', control, hint);
            $('.ledcaption', el).append(element('span', 'leddot ledpreview'), element('span', 'ledplaying'));
            return el;
        };
        const nowTag = () => element('span', 'lednow', 'now');
        const numberCell = () => {
            const el = element('span', 'lednum');
            el.append(element('span', 'ledno'), nowTag());
            return el;
        };
        // The line of a closed rule: what it reacts to, and how the LED looks then
        const summaryButton = () => {
            const el = button('ledsum');
            el.setAttribute('aria-expanded', 'false');
            const title = element('span', 'ledsumtitle');
            title.append(element('span', 'ledsumname'), element('span', 'ledsumwhen'));
            const detail = element('span', 'ledsumdetail');
            const look = element('span', 'ledsumlook');
            look.append(element('span', 'leddot'), element('span', 'ledsumeffect'));
            detail.append(look, ' ', element('span', 'ledsumrhythm'));
            el.append(title, detail);
            return el;
        };
        const moveButton = (direction, text) => {
            const el = button('ledmove ' + direction, text);
            el.dataset.key = direction === 'up' ? 'ArrowUp' : 'ArrowDown';
            return el;
        };
        const hex = input => input.value.replace('#', '').toLowerCase();
        // A rule that is off has no color, so a color left over from another effect doesn't count as a change
        const colorOf = row => $('.ledeffect', row).value === 'off' ? '000000' : hex($('.ledcolor', row));
        const effectOption = effect => $$('option', effectTemplate.content).find(option => option.value === effect);
        // The name of an effect, without its rhythm, as the hub names it
        const effectLabel = effect => (effectOption(effect) || { textContent: '' }).textContent.split(' (')[0];
        // How long the hub plays an effect for a test
        const effectSeconds = effect => Number((effectOption(effect) || { dataset: {} }).dataset.seconds) || 3;
        const testSeconds = row => effectSeconds($('.ledeffect', row).value);

        const parse = text => {
            const rules = [];
            let otherwise = { effect: 'off', color: '000000' };
            for (const entry of text.split(';')) {
                const entryFields = entry.split(',').map(field => field.trim());
                if (entryFields.length !== 5 || !effectIds.has(entryFields[3]) || !/^[0-9a-fA-F]{6}$/.test(entryFields[4])) {
                    continue;
                }
                const rule = { trigger: entryFields[0], timing: entryFields[1], seconds: Number(entryFields[2]) || 0, effect: entryFields[3], color: entryFields[4].toLowerCase() };
                if (rule.trigger === '*') {
                    otherwise = rule;
                } else if (triggerIds.has(rule.trigger) && rules.length < max) {
                    rules.push(rule);
                }
            }
            return { rules, otherwise };
        };

        const list = element('ol', 'ledlist');
        list.setAttribute('role', 'list');
        const empty = element('p', 'ledempty', 'No rules: the LED always shows Otherwise.');
        const otherwiseRow = element('div', 'ledrule ledotherwise');
        const live = element('div', 'ledlive');
        live.setAttribute('role', 'status');
        const paletteList = element('datalist');
        paletteList.id = 'ledpalette';
        paletteList.append(...palette.map(color => {
            const option = element('option');
            option.value = '#' + color;
            option.label = colorName(color);
            return option;
        }));
        ledRules.append(list, empty, otherwiseRow, live, paletteList);
        const rows = () => $$('.ledrule', list);
        const nameOf = row => $('.ledtrigger', row).selectedOptions[0].textContent;
        // The warning of each rule, and the one a screen reader last heard of, so that an announcement also tells what
        // an edit did to the other rules
        const warnings = new WeakMap();
        const heard = new WeakMap();
        // The rules that each rule waits for, where that is its warning, and those it waited for when last heard of
        const waitsOn = new WeakMap();
        const heardWaits = new WeakMap();
        const hearAll = () => rows().forEach(row => {
            heard.set(row, warnings.get(row));
            heardWaits.set(row, waitsOn.get(row) || []);
        });
        // How much a warning matters: 2 the rule never shows, 1 it waits, is cut short or is too dark to see
        const level = warning => !warning ? 0 : warning.startsWith('Never shows') ? 2 : 1;
        const added = row => (waitsOn.get(row) || []).filter(other => !(heardWaits.get(row) || []).includes(other));
        const say = (text, edited) => {
            // A warning that got worse, or a rule that now waits for one more. One that eased isn't news.
            const fresh = rows().filter(row => level(warnings.get(row)) > level(heard.get(row)) || (level(warnings.get(row)) > 0 && added(row).length > 0));
            // A rule that all of them now wait for is named once, which also says what caused their warnings: the rule
            // that was just moved or changed, when they all wait for it
            const common = fresh.length > 1 ? added(fresh[0]).filter(other => fresh.every(row => added(row).includes(other))) : [];
            const cause = common.includes(edited) ? edited : common[0];
            // The cause first, then one outcome each for the rules that wait and for those that are missed
            const missed = fresh.filter(row => fleeting(readRule(row))).length;
            const waiting = fresh.length - missed;
            const outcome = [waiting ? waiting + (waiting === 1 ? ' rule now waits' : ' rules now wait') : '',
                missed ? missed + (waiting ? '' : missed === 1 ? ' rule' : ' rules') + (missed === 1 ? ' is' : ' are') + ' now missed' : ''];
            const lines = cause ? ['While "' + nameOf(cause) + '" applies, ' + outcome.filter(Boolean).join(' and ') + '.']
                : fresh.map(row => '"' + nameOf(row) + '": ' + warnings.get(row));
            hearAll();
            const all = [text, ...lines.slice(0, 3), lines.length > 3 ? (lines.length - 3) + ' more rules got a warning.' : ''].filter(Boolean).join(' ');
            if (all) {
                live.textContent = '';
                setTimeout(() => { live.textContent = all; }, 40);
            }
        };

        const updateTiming = row => {
            const trigger = $('.ledtrigger', row);
            const timing = $('.ledtiming select', row);
            const seconds = $('.ledtiming input', row);
            const isEvent = eventTriggers.has(trigger.value);
            for (const option of timing.options) {
                option.disabled = isEvent && option.value !== 'for';
            }
            if (isEvent && timing.value !== 'for') {
                // Kept, so that a rule passing through this trigger with the arrow keys gets its timing back
                row.dataset.timing = timing.value;
                timing.value = 'for';
            } else if (!isEvent && row.dataset.timing) {
                timing.value = row.dataset.timing;
                delete row.dataset.timing;
            }
            const timed = timing.value !== 'while';
            seconds.hidden = !timed;
            seconds.disabled = !timed;
            $('.ledunit', row).hidden = !timed;
            if (timed && !(Number(seconds.value) > 0)) {
                seconds.value = seconds.dataset.good;
            }
        };

        const ruleRow = rule => {
            const row = element('li', 'ledrule');
            const timing = element('span', 'ledtiming');
            const seconds = element('input');
            seconds.type = 'number';
            seconds.min = 1;
            seconds.max = 65535;
            seconds.step = 1;
            seconds.required = true;
            seconds.inputMode = 'numeric';
            seconds.value = rule.seconds > 0 ? rule.seconds : 10;
            seconds.dataset.good = seconds.value;
            timing.append(select('', timings.map(([id, label]) => '<option value="' + id + '">' + label + '</option>').join(''), rule.timing, 'Timing'), seconds, element('span', 'ledunit', 's'));
            // A slider, because screen readers pass the arrow keys on to one, but not to a button
            const handle = button('ledhandle');
            handle.setAttribute('role', 'slider');
            handle.setAttribute('aria-orientation', 'vertical');
            handle.setAttribute('aria-valuemin', 1);
            handle.append(element('span', 'ledgrip'));
            const actions = element('div', 'ledruleactions');
            actions.append(moveButton('up', 'Move up'), moveButton('down', 'Move down'), button('ledremove', 'Remove rule'));
            // In reading order on every screen width, so that the tab order follows the layout
            row.append(handle, numberCell(), summaryButton());
            if (canTest) {
                row.append(button('ledtest', 'Test'));
            }
            const trigger = select('ledtrigger', triggerTemplate.innerHTML, rule.trigger, 'When');
            const triggerHint = hintElement();
            const timingHint = hintElement();
            const effect = select('ledeffect', effectTemplate.innerHTML, rule.effect, 'Show');
            const effectHint = hintElement();
            const note = noteElement();
            // As descriptions, so that the names of the controls stay short
            trigger.setAttribute('aria-describedby', triggerHint.id + ' ' + noteIds(note));
            $('select', timing).setAttribute('aria-describedby', timingHint.id);
            seconds.setAttribute('aria-describedby', timingHint.id);
            effect.setAttribute('aria-describedby', effectHint.id);
            $('.ledsum', row).setAttribute('aria-describedby', noteIds(note));
            row.append(
                field('label', 'when', 'When', trigger, triggerHint),
                field('div', 'timing', 'Timing', timing, timingHint),
                showField(effect, effectHint),
                field('label', 'color', 'Color', colorInput(rule.color, 'Color')),
                note,
                actions);
            updateTiming(row);
            return row;
        };

        const readRule = row => {
            const timing = $('.ledtiming select', row).value;
            const seconds = $('.ledtiming input', row);
            return {
                trigger: $('.ledtrigger', row).value,
                timing,
                // Seconds that aren't right keep the last good value until they are. valueAsNumber reads "1e3" as 1000,
                // where parseInt would read 1.
                seconds: timing === 'while' ? 0 : seconds.validity.valid ? seconds.valueAsNumber : Number(seconds.dataset.good),
                effect: $('.ledeffect', row).value,
                color: colorOf(row)
            };
        };

        // As the list is stored: no seconds while it applies, and no color for a rule that keeps the LED dark
        const entryText = rule => [rule.trigger, rule.timing, rule.timing === 'while' ? 0 : rule.seconds, rule.effect, rule.effect === 'off' ? '000000' : rule.color].join(',');
        const entryOf = row => row === otherwiseRow ? ['*', 'while', 0, $('.ledeffect', row).value, colorOf(row)].join(',') : entryText(readRule(row));

        const serialize = () => {
            value.value = [...rows(), otherwiseRow].map(entryOf).join(';');
        };

        // True when rule a applies whenever rule b would, so that b can never show below a. A "for" rule can outlast
        // its state, so a "while" rule above it doesn't hide it.
        const covers = (a, b) => a.trigger === b.trigger && (
            (a.timing === 'while' && b.timing !== 'for') ||
            (a.timing === 'after' && b.timing === 'after' && a.seconds <= b.seconds) ||
            (a.timing === 'for' && b.timing === 'for' && a.seconds >= b.seconds));

        // What keeps each rule from showing. A rule that can never show keeps nothing else from showing.
        const analyze = rules => {
            const winners = rules.map((rule, index) => rules.slice(0, index).findIndex(other => covers(other, rule)));
            const alive = rules.map((rule, index) => !disabledTriggers.has(rule.trigger) && winners[index] < 0);
            const waitsFor = rules.map((rule, index) => alive[index] ? rules.slice(0, index).flatMap((other, i) => alive[i] && hides(other, rule) ? [i + 1] : []) : []);
            // A lock is nearly always locked or unlocked, and an open door nearly always unlocked, so what waits for
            // those hardly ever gets its turn
            const blocked = rules.map((rule, index) => {
                const waits = waitsFor[index];
                const lasting = states => waits.filter(number => states.includes(rules[number - 1].trigger) && rules[number - 1].timing !== 'for');
                const door = rule.trigger === 'dooropen' ? waits.filter(number => rules[number - 1].trigger === 'unlocked' && rules[number - 1].timing === 'while') : [];
                const lock = lasting(['locked']).length && lasting(['unlocked']).length ? lasting(['locked', 'unlocked']) : [];
                // A rule set to Dark still applies, for as long as its state lasts
                const dark = waits.filter(number => rules[number - 1].effect === 'off' && !fleeting(rules[number - 1]));
                // Everything that holds it back, so that one move above all of it puts the rule right
                const by = waits.filter(number => door.includes(number) || lock.includes(number) || dark.includes(number) || holdsBack(rules[number - 1], rule));
                return by.length ? { reason: door.length ? 'door' : lock.length ? 'lock' : '', by, lock, door, dark } : null;
            });
            return { winners, alive, blocked };
        };
        // An earlier rule with the same trigger takes over from a "for the first" rule
        const overtakers = (rules, index) => {
            const rule = rules[index];
            return rule.timing !== 'for' ? [] : rules.slice(0, index).flatMap((other, i) => other.trigger === rule.trigger
                && (other.timing === 'while' || (other.timing === 'after' && other.seconds < rule.seconds)) ? [i] : []);
        };
        // The level (see level) of the warning that refresh gives each rule, for the saved list, which isn't on the page
        const levelsOf = rules => {
            const { winners, blocked } = analyze(rules);
            return rules.map((rule, index) => disabledTriggers.has(rule.trigger) || winners[index] >= 0 ? 2
                : (rule.effect !== 'off' && colorName(rule.color) === 'black') || overtakers(rules, index).length || blocked[index] ? 1 : 0);
        };

        // The words of a note about the order that say how much it matters, which stand out so that the list can be scanned.
        // Where the open rule names the rules that the list only counts (open), the rest comes twice, one for each.
        const withVerdict = (text, open) => {
            const lead = (/^(Never shows:|Rarely shows:|Can wait for hours|Can be missed|Cut short after \d+ s)/.exec(text) || [''])[0];
            const rest = open && open !== text && open.startsWith(lead) ? [element('span', 'lednoteshut', text.slice(lead.length)), element('span', 'lednotemore', open.slice(lead.length))]
                : [text.slice(lead.length)];
            return lead ? [element('strong', 'lednotelead', lead), ...rest] : rest;
        };
        // detail and lines are only for the open rule, so that a long list doesn't fill up with remarks about its order:
        // detail right after the text, such as which rules it waits for, and lines each on a line of its own, in grey
        // where they are a remark and not a problem. fix is a button that puts a problem right, right after the text of
        // that problem (fix.after, or else the text). never: the rule can't show at all, which looks worse than a wait.
        // open: the text as the open rule says it, where it names the rules that the list counts. A test that failed
        // comes first, on a line of its own, so that it doesn't hide what the order does to the rule.
        const setNote = (row, text, quiet, lines, fix, never, detail, open) => {
            const note = $('.lednote', row);
            const error = row.dataset.error || '';
            const main = text || '';
            const after = (detail || []).filter(Boolean);
            const more = (lines || []).filter(line => line.text);
            // How many lines come before the button
            const split = !fix ? more.length : fix.after ? more.findIndex(line => line.text === fix.after) + 1 : 0;
            const key = [error, main, open, ...after, '#', ...more.map(line => line.text), split, fix ? fix.text : ''].join('|');
            if (note.dataset.key !== key) {
                note.dataset.key = key;
                // In two elements, which the controls of the rule point at, the first one also the button
                const words = element('span', 'lednotetext');
                words.id = note.dataset.textId;
                const rest = element('span', 'lednotetext');
                rest.id = note.dataset.restId;
                if (error) {
                    const failed = element('span', 'lednoteerror');
                    failed.append(...withLogin(error));
                    words.append(failed);
                }
                // Under an error, the problem with the order gets a line and a dot of its own. The space ends the line
                // of the error, where it doesn't show.
                const problem = error && main ? element('span', 'lednotewarn' + (never ? ' never' : '')) : words;
                if (problem !== words) {
                    words.append(' ', problem);
                }
                if (main) {
                    problem.append(...withVerdict(main, open));
                }
                problem.append(...after.map(line => element('span', 'lednotemore', ' ' + line)));
                more.forEach((line, index) => {
                    // Each on a line, with a space that the line break hides, so that a screen reader doesn't run the
                    // sentences together. Without a text before it the first one starts next to the dot.
                    const first = !main && !error && !index;
                    const el = element('span', 'lednotemore' + (first ? '' : ' lednoteline') + (line.remark ? ' lednoteremark' : ''), first ? '' : ' ');
                    el.append(...withVerdict(line.text));
                    (index < split ? problem : rest).append(el);
                });
                note.replaceChildren(words);
                if (fix) {
                    const fixButton = button('lednotefix', fix.text);
                    fixButton.setAttribute('aria-describedby', words.id);
                    note.append(fixButton);
                }
                note.append(rest);
            }
            if (fix) {
                const fixButton = $('.lednotefix', note);
                fixButton.dataset.to = fix.to + 1;
                fixButton.dataset.side = fix.side;
            }
            note.hidden = !error && !main && !more.length;
            note.classList.toggle('onlyopen', !error && !main);
            note.classList.toggle('error', Boolean(error));
            note.classList.toggle('quiet', Boolean(quiet && !error));
            note.classList.toggle('never', Boolean(never && !error));
        };

        // In one piece, so that a narrow line breaks before it and not inside it
        const timingText = rule => rule.timing === 'after' ? 'after\u00A0' + rule.seconds + '\u00A0s' : rule.timing === 'for' ? 'for\u00A0the\u00A0first\u00A0' + rule.seconds + '\u00A0s' : '';
        const summarize = (row, title, when, prefix) => {
            const effect = $('.ledeffect', row);
            const text = effect.selectedOptions[0].textContent;
            const [effectName, rhythm] = text.split(' (');
            const color = effect.value === 'off' ? '' : $('.ledcolor', row).value;
            const word = color && colorName(color.slice(1));
            const dot = $('.leddot', row);
            $('.ledsumname', row).textContent = title;
            $('.ledsumwhen', row).textContent = when ? ', ' + when : '';
            $('.ledsumeffect', row).textContent = word ? effectName + ' ' + word : effectName;
            // On a phone the field is narrow, and the name of the color alone has to do
            const caption = $('.ledfieldcolor .ledcaption', row);
            if (caption.textContent !== (word ? 'Color: ' + word : 'Color')) {
                caption.replaceChildren(...(word ? [element('span', 'ledcaptionprefix', 'Color: '), word] : ['Color']));
            }
            $('.ledsumrhythm', row).textContent = rhythm ? '(' + rhythm : '';
            // Keeps its place without a color, so that the names of the effects line up
            dot.style.visibility = color ? '' : 'hidden';
            dot.style.background = color;
            const preview = $('.ledpreview', row);
            preview.style.visibility = dot.style.visibility;
            preview.style.background = color;
            preview.dataset.effect = effect.value;
            // The notes of the rule are its description
            // In the order of the visible text, so that it can be spoken as it reads
            $('.ledsum', row).dataset.label = prefix + [title, when, (word ? effectName + ' ' + word : effectName) + (rhythm ? ' (' + rhythm : '')].filter(Boolean).join(', ');
        };

        let saved = null;
        let saving = false;
        // The edits of a Save that failed or got no answer, or that came back after a reload, and what to say about them
        let failed = null;
        let unsavedText = '';
        // The rules whose warning is new or worse than in the saved list, which the Save bar counts so that a Save doesn't
        // pass over them unseen, with a button that goes to them
        let warned = [];
        const warnedGo = button('');
        // The saved rules with the level of each, in the order of the list
        let savedLevels = null;
        const levelsBefore = () => {
            if (!savedLevels || savedLevels.rules !== saved.rules) {
                const rules = parse(saved.rules).rules;
                const levels = levelsOf(rules);
                savedLevels = { rules: saved.rules, list: rules.map((rule, index) => ({ entry: entryText(rule), trigger: rule.trigger, level: levels[index] })) };
            }
            return savedLevels.list.slice();
        };
        const pageState = () => ({ rules: value.value, brightness: fields.LEDBRI.value, order: fields.LEDORD.value });
        const sameState = (a, b) => a.rules === b.rules && a.brightness === b.brightness && a.order === b.order;
        const isUnsaved = () => saved !== null && !sameState(pageState(), saved);
        // What this page last had the LED show for a test, and until when the hub can still report it
        let sent = null;
        // The effect that the hub reports a test of, or nothing
        let testShown = '';
        // Under "Now showing", which follows the saved rules like the LED does, or during a test what comes after it. Its
        // line is kept free, so that the first edit doesn't move the page under the pointer.
        const shownSaved = shownDot ? element('span', 'ledshownsaved') : null;
        if (shownSaved) {
            shownDot.parentNode.append(shownSaved);
        }
        const refreshShownSaved = () => {
            if (shownSaved) {
                const text = testShown ? 'Back to the saved rules after the test.' : 'The LED follows the saved rules until you save.';
                if (shownSaved.textContent !== text) {
                    shownSaved.textContent = text;
                }
                shownSaved.style.visibility = testShown || (saved !== null && value.value !== saved.rules) ? '' : 'hidden';
            }
        };
        // For a few seconds after a Save that reached the hub, until the next edit
        let justSaved = false;
        const refreshUnsaved = () => {
            const unsaved = isUnsaved();
            if (unsaved) {
                justSaved = false;
            }
            refreshShownSaved();
            const held = Boolean(failed) && sameState(pageState(), failed.state);
            const text = unsaved ? (held ? failed.text : 'Not saved yet') : justSaved ? 'Saved' : '';
            const saveFailed = unsaved && held && failed.retry;
            const count = unsaved && !saveFailed ? warned.length : 0;
            // Without new warnings: saving on the GPIO page restarts the hub, so the question comes up right here
            const more = justSaved ? (canTest ? 'The LED follows these rules now' : '') : saveFailed ? 'Try Save again'
                : count ? 'Go to ' + (count === 1 ? 'the new warning' : count + ' new warnings') : 'No restart needed';
            if (unsavedText !== text + '|' + more) {
                unsavedText = text + '|' + more;
                unsavedLabel.replaceChildren(...(text ? [text] : []));
                unsavedLabel.classList.toggle('ledsaved', justSaved);
                if (text && more) {
                    const small = element('small', '', count ? '' : more);
                    if (count) {
                        warnedGo.textContent = more;
                        small.append(warnedGo);
                    }
                    unsavedLabel.append(small);
                }
            }
            discardButton.hidden = !unsaved;
        };
        // In another tab, which leaves this page and its edits as they are. The login ends on the overview, and Save
        // would lead to it too, with the edits kept only for the next visit of this page.
        const expiredText = () => 'Your login has expired. Log in again in a new tab, then come back' + (isUnsaved() ? ': your edits stay here.' : '.');
        // "Log in again" in a message is a link
        const withLogin = text => {
            const at = text.indexOf('Log in again');
            if (at < 0) {
                return [text];
            }
            const link = element('a', '', 'Log in again');
            link.href = '/';
            link.target = '_blank';
            return [text.slice(0, at), link, text.slice(at + 12)];
        };

        // The row the LED follows. Found by its content, because the hub numbers the saved rules and the list on the
        // page can already differ from them.
        let shownRule = -1;
        const shownEntry = () => {
            const entries = saved ? saved.rules.split(';') : [];
            return entries[shownRule === 0 ? entries.length - 1 : shownRule - 1];
        };
        const markShown = () => {
            const entry = shownEntry();
            const all = [...rows(), otherwiseRow];
            const match = entry ? all.find(row => entryOf(row) === entry) : null;
            all.forEach(row => {
                row.classList.toggle('ledshowing', row === match);
                const line = $('.ledsum', row);
                line.setAttribute('aria-label', (row === match ? 'On the LED now. ' : '') + line.dataset.label);
            });
            // From the saved rule, because that is what the LED follows while the list on the page differs. A test of the
            // effect that this page sent last is drawn from what it sent, because the hub only names the effect.
            if (shownDot) {
                const own = testShown && sent && Date.now() < sent.until && effectLabel(sent.effect) === testShown;
                const [effect, color] = own ? [sent.effect, sent.color] : entry ? entry.split(',').slice(3) : [];
                shownDot.hidden = !effect || effect === 'off';
                shownDot.style.background = shownDot.hidden ? '' : '#' + color;
                shownDot.dataset.effect = shownDot.hidden ? '' : effect;
                shownColor.textContent = shownDot.hidden ? '' : ' ' + colorName(color);
                shownWhy.hidden = shownRule !== 0;
            }
            refreshShownSaved();
        };

        const refresh = () => {
            const all = rows();
            const rules = all.map(readRule);
            const names = all.map(row => $('.ledtrigger', row).selectedOptions[0].textContent);
            const unavailable = all.map(row => $('.ledtrigger', row).selectedOptions[0].dataset.off);
            const { winners, alive, blocked } = analyze(rules);
            // The rules that each rule keeps waiting, by number
            const holds = rules.map((rule, index) => blocked.flatMap((entry, i) => entry && entry.by.includes(index + 1) ? [i + 1] : []));
            // By name, because the numbers change with every move and the rule can be a screen away
            const quoted = numbers => [...new Set(numbers.map(number => '"' + names[number - 1] + '"'))];
            const anyOf = numbers => {
                const items = quoted(numbers);
                return items.length < 3 ? items.join(' or ') : items[0] + ' or any of ' + (items.length - 1) + ' more';
            };
            const allOf = items => items.length < 3 ? items.join(' and ') : items[0] + ' and ' + (items.length - 1) + ' more';
            // Every button moves its own rule, so that it does what it says where it is
            const moveTo = (index, to, side) => ({ to, side, text: 'Move this one ' + (side === 'before' ? 'above ' : 'below ') + (names[to] === names[index] ? 'the other rule' : '"' + names[to] + '"') });
            // Two states that look alike can't be told apart on the LED. An update and a start are over in moments.
            const lookOf = (effect, color) => effect === 'off' ? '' : effect + ' ' + colorName(color);
            const looks = rules.map((rule, index) => alive[index] && !briefTriggers.has(rule.trigger) ? lookOf(rule.effect, rule.color) : '');
            const otherwiseLook = lookOf($('.ledeffect', otherwiseRow).value, colorOf(otherwiseRow));
            const twins = (look, trigger) => look ? rules.flatMap((other, i) => looks[i] === look && other.trigger !== trigger ? [i + 1] : []) : [];
            all.forEach((row, index) => {
                const number = index + 1;
                const rule = rules[index];
                const above = rules.slice(0, index);
                // What keeps the rule from showing, the worst first. The list says the first in short, in the same words
                // whether the order is saved or not, so that it can be scanned. The open rule says which rules and why
                // (more), and has one button (move).
                const issues = [];
                if (unavailable[index]) {
                    issues.push({ text: 'Never shows: ' + unavailable[index] + '.', never: true });
                } else if (winners[index] >= 0) {
                    const coverers = above.flatMap((other, i) => covers(other, rule) ? [i] : []);
                    // Swapping two rules with the same timing would only hide the other one
                    const twin = coverers.some(other => covers(rule, rules[other]));
                    issues.push({
                        text: 'Never shows: a rule above always applies first.',
                        never: true,
                        more: [twin ? 'The other "' + names[index] + '" rule has the same timing: remove one, or change the timing of one.' : ''],
                        move: twin ? null : moveTo(index, coverers[0], 'before')
                    });
                } else {
                    if (rule.effect !== 'off' && colorName(rule.color) === 'black') {
                        issues.push({ text: blackNote });
                    }
                    const overtaken = overtakers(rules, index);
                    if (overtaken.length) {
                        const first = rules[overtaken[0]];
                        issues.push({
                            text: (first.timing === 'while' ? 'Rarely shows: the other "' + names[index] + '" rule above comes first.'
                                : 'Cut short after ' + first.seconds + ' s by the other "' + names[index] + '" rule above.'),
                            move: moveTo(index, overtaken[0], 'before')
                        });
                    }
                    // A state that lasts above a problem or a moment. That a rule waits for any other is what the order is for.
                    // A "for the first" rule, or a state that is over in seconds, doesn't wait: what happens meanwhile is missed.
                    const entry = blocked[index];
                    if (entry) {
                        // The rules that the short text names in words
                        const named = entry.reason === 'lock' ? entry.lock : entry.reason === 'door' ? entry.door : [];
                        const others = entry.by.filter(number => !named.includes(number));
                        // Dark when all of them are, so that the list says why
                        const kind = entry.dark.length === entry.by.length ? ' Dark rule' : ' rule';
                        const missed = fleeting(rule);
                        // The list says how many rules, and the open rule which ones instead. Then the reason of the short
                        // text, the other rules, and for a rule that is missed why it doesn't get its turn.
                        issues.push({
                            text: entry.reason ? 'Rarely shows: ' + (missed ? 'missed' : 'waits') + ' while the lock is ' + (entry.reason === 'lock' ? 'locked or unlocked.' : 'unlocked.')
                                : missed ? 'Can be missed while ' + (entry.by.length > 1 ? 'one of ' + entry.by.length + kind + 's above' : 'a' + kind + ' above') + ' applies.'
                                    : 'Can wait for hours behind ' + (entry.by.length > 1 ? entry.by.length + kind + 's' : 'a' + kind) + ' above.',
                            open: entry.reason ? '' : (missed ? 'Can be missed' : 'Can wait for hours') + ' while ' + anyOf(entry.by) + ' applies.',
                            more: [entry.reason === 'lock' ? 'A lock is nearly always one or the other.' : entry.reason === 'door' ? 'An open door is nearly always unlocked.' : '',
                                named.length && others.length ? 'It ' + (missed ? 'is also missed' : 'also waits') + ' while ' + anyOf(others) + ' applies.' : '',
                                !missed ? '' : 'It ' + (rule.timing !== 'for' ? 'lasts only seconds'
                                    : 'only shows ' + (eventTriggers.has(rule.trigger) ? 'for ' : 'in its first ') + rule.seconds + ' s') + ', so it is over before its turn.',
                                entry.dark.length ? 'A rule set to Dark still counts: it keeps the LED off.' : ''],
                            // Above all of them
                            move: moveTo(index, entry.by[0] - 1, 'before'),
                            by: entry.by
                        });
                    }
                }
                const listed = issues[0];
                const warning = listed ? listed.text : '';
                warnings.set(row, warning);
                waitsOn.set(row, listed && listed.by ? listed.by.map(other => all[other - 1]) : []);
                const same = allOf([...quoted(twins(looks[index], rule.trigger)), ...(looks[index] && looks[index] === otherwiseLook ? ['Otherwise'] : [])]);
                // The other side of such a wait, for whoever moved or changed this rule: which rules wait while it applies,
                // and a button that moves it below them. Only in the open rule, so that the list names each wait once.
                const held = holds[index];
                // One outcome each for the rules that wait and those that are missed
                const outcomeOf = (numbers, one, many) => {
                    const items = quoted(numbers);
                    return items.length ? allOf(items) + (items.length === 1 ? one : many) : '';
                };
                const holdText = !held.length ? '' : 'While this applies, ' + [outcomeOf(held.filter(number => !fleeting(rules[number - 1])), ' waits', ' wait'),
                    outcomeOf(held.filter(number => fleeting(rules[number - 1])), ' is missed', ' are missed')].filter(Boolean).join(', and ') + '.';
                // Each problem in one piece: what the open rule says of it, then which rules and why. Then the remarks,
                // which aren't problems of this rule.
                const lineOf = issue => [issue.open || issue.text, ...(issue.more || [])].filter(Boolean).join(' ');
                const lines = issues.filter(issue => issue !== listed).map(issue => ({ text: lineOf(issue) }))
                    .concat({ text: holdText, remark: true }, { text: same ? 'Same look as ' + same + '.' : '', remark: true });
                // For the first problem that a move puts right, or else for the rules that wait for this one, and right
                // after its text
                const fixing = issues.find(issue => issue.move);
                const fix = fixing ? fixing.move : held.length ? moveTo(index, held[held.length - 1] - 1, 'after') : null;
                if (!fixing && held.length > 1) {
                    fix.text = 'Move this one below them';
                }
                if (fix) {
                    fix.after = !fixing ? holdText : fixing === listed ? '' : lineOf(fixing);
                }
                // Seconds that aren't right get a message, the same as the browser's, which says what the rule does until
                // then. A message from before would keep the field wrong.
                const seconds = $('.ledtiming input', row);
                seconds.setCustomValidity('');
                const wrong = seconds.disabled || seconds.validity.valid ? '' : 'Choose 1 to 65535 s (about 18 hours)'
                    + (eventTriggers.has(rule.trigger) ? '' : rule.timing === 'after' ? ', or "while it applies" for right away' : ', or "while it applies" for as long as it lasts')
                    + '. Until then the rule keeps ' + rule.seconds + ' s.';
                seconds.setCustomValidity(wrong);
                // Looks wrong too, but not while it is empty and still being typed in
                const timingHint = $('.ledfieldtiming .ledfieldhint', row);
                markWrong(seconds, timingHint, Boolean(wrong) && !(seconds.value === '' && seconds === document.activeElement));
                // Next to where they are chosen: what the trigger covers, and what the timing does, because a "for" rule on
                // a state that is already on seems dead
                setHint($('.ledfieldwhen .ledfieldhint', row), $('.ledtrigger', row).selectedOptions[0].dataset.applies || '');
                setHint(timingHint, wrong || (rule.timing === 'after' ? 'Shows once this has lasted ' + rule.seconds + ' s.'
                    : rule.timing !== 'for' ? ''
                        : eventTriggers.has(rule.trigger) ? 'Shows for ' + rule.seconds + ' s each time this happens.'
                            : 'Shows for the first ' + rule.seconds + ' s after this starts, even if it ends sooner.'));
                // Dark isn't the same as no rule, which is easy to mistake. Which rules wait is in the note.
                setHint($('.ledfieldshow .ledfieldhint', row), rule.effect !== 'off' ? '' : 'The LED stays off while this applies. To ignore this, remove the rule.');
                $('.ledno', row).textContent = number;
                const handle = $('.ledhandle', row);
                name(handle, 'Move "' + names[index] + '" (drag, or use the arrow keys)');
                const [moveUp, moveDown] = $$('.ledmove', row);
                moveUp.disabled = index === 0;
                moveDown.disabled = index === all.length - 1;
                name(moveUp, 'Move up: rule ' + number);
                name(moveDown, 'Move down: rule ' + number);
                handle.setAttribute('aria-valuemax', all.length);
                handle.setAttribute('aria-valuenow', all.length - index);
                handle.setAttribute('aria-valuetext', 'position ' + number + ' of ' + all.length);
                [['.ledtrigger', 'when'], ['.ledtiming select', 'timing'], ['.ledtiming input', 'seconds'], ['.ledeffect', 'show'], ['.ledcolor', 'color ' + colorName(rule.color)]]
                    .forEach(([selector, word]) => $(selector, row).setAttribute('aria-label', 'Rule ' + number + ': ' + word));
                if (canTest) {
                    name($('.ledtest', row), 'Test rule ' + number + ' on the LED for ' + testSeconds(row) + ' seconds');
                }
                name($('.ledremove', row), 'Remove rule ' + number);
                $('.ledcolor', row).disabled = rule.effect === 'off';
                // A rule that never shows has its look faded in the list
                row.classList.toggle('lednever', Boolean(listed && listed.never));
                setNote(row, warning, !listed, lines, fix, listed && listed.never, listed && listed.more, listed && listed.open);
                summarize(row, names[index], timingText(rule), 'Rule ' + number + ': ');
            });
            $('.ledcolor', otherwiseRow).disabled = $('.ledeffect', otherwiseRow).value === 'off';
            $('.ledcolor', otherwiseRow).setAttribute('aria-label', 'Otherwise: color ' + colorName(hex($('.ledcolor', otherwiseRow))));
            if (canTest) {
                name($('.ledtest', otherwiseRow), 'Test Otherwise on the LED for ' + testSeconds(otherwiseRow) + ' seconds');
            }
            const otherwiseBlack = $('.ledeffect', otherwiseRow).value !== 'off' && colorName(hex($('.ledcolor', otherwiseRow))) === 'black';
            const otherwiseSame = allOf(quoted(twins(otherwiseLook)));
            setNote(otherwiseRow, otherwiseBlack ? blackNote : '', !otherwiseBlack, [{ text: otherwiseSame ? 'Same look as ' + otherwiseSame + '.' : '', remark: true }]);
            summarize(otherwiseRow, 'Otherwise', '', '');
            empty.hidden = all.length > 0;
            addButton.disabled = all.length >= max;
            countLabel.textContent = all.length + ' of ' + max + ' rules';
            // Rule by rule against the saved list: the same settings are the same rule, and else the same trigger is, so
            // that a new color or timing doesn't make a warning new. A rule the saved list has no rule left for is new.
            const left = saved ? levelsBefore() : [];
            const take = test => {
                const at = left.findIndex(test);
                return at < 0 ? null : left.splice(at, 1)[0];
            };
            const exact = rules.map(rule => take(other => other.entry === entryText(rule)));
            const before = rules.map((rule, index) => exact[index] || take(other => other.trigger === rule.trigger));
            warned = saved ? all.filter((row, index) => level(warnings.get(row)) > (before[index] ? before[index].level : 0)) : [];
            serialize();
            refreshUnsaved();
            markShown();
        };

        // The last removal or replaced list can be undone until the next change
        let undo = null;
        const dismissUndo = () => {
            if (undo) {
                undo.remove();
                undo = null;
            }
        };
        // The Undo of a removal puts one rule back and leaves later edits alone. Undoing a replaced or discarded list,
        // or a changed order, would overwrite them.
        const dismissListUndo = () => {
            if (undo && (!list.contains(undo) || undo.dataset.order)) {
                dismissUndo();
            }
        };
        const undoNotice = (text, restore, tag) => {
            dismissUndo();
            undo = element(tag || 'li', 'ledundo');
            const undoButton = button('', 'Undo', 'Undo: ' + text);
            undoButton.addEventListener('click', restore);
            undo.append(element('span', '', text), undoButton);
            return undo;
        };

        const load = text => {
            dismissUndo();
            const parsed = parse(text);
            list.replaceChildren(...parsed.rules.map(ruleRow));
            otherwiseRow.classList.remove('open');
            otherwiseRow.replaceChildren(numberCell(), summaryButton());
            if (canTest) {
                otherwiseRow.append(button('ledtest', 'Test'));
            }
            const otherwiseNote = noteElement();
            otherwiseRow.append(
                showField(select('ledeffect', effectTemplate.innerHTML, parsed.otherwise.effect, 'Otherwise: show')),
                field('label', 'color', 'Color', colorInput(parsed.otherwise.color, 'Otherwise: color')),
                otherwiseNote);
            $('.ledsum', otherwiseRow).setAttribute('aria-describedby', otherwiseNote.dataset.textId);
            refresh();
            // A new list isn't an edit of the rules in it
            hearAll();
        };

        const removeRule = row => {
            const notice = undoNotice('"' + $('.ledtrigger', row).selectedOptions[0].textContent + '" removed.', () => {
                notice.replaceWith(row);
                undo = null;
                // Another rule may have opened since
                if (row.classList.contains('open')) {
                    openRule(row);
                }
                refresh();
                $('.ledhandle', row).focus();
                say('', row);
            });
            delete row.dataset.error;
            row.replaceWith(notice);
            refresh();
            $('button', notice).focus();
            // The button says what happened, this what it did to the other rules
            say('');
        };

        const ledRequest = (effect, color) => {
            const controller = new AbortController();
            const timeout = setTimeout(() => controller.abort(), 5000);
            return fetch('/get?' + new URLSearchParams({ page: 'ledtest', effect, color, brightness: fields.LEDBRI.value, order: fields.LEDORD.value }), { cache: 'no-store', signal: controller.signal })
                .then(response => response.text().then(text => ({ response, text })))
                .catch(() => { throw new Error('Nuki Hub doesn\'t answer. Check the connection and try again.'); })
                .finally(() => clearTimeout(timeout))
                .then(({ response, text }) => {
                    // An expired login answers with a redirect to the login page, which fetch follows
                    if (response.redirected) {
                        throw new Error(expiredText());
                    }
                    if (!response.ok) {
                        const plain = (response.headers.get('Content-Type') || '').startsWith('text/plain');
                        throw new Error(plain && text ? text : 'Nuki Hub answered with an error (HTTP ' + response.status + ').');
                    }
                    if (text !== 'OK') {
                        throw new Error('Nuki Hub sent an unexpected answer. Reload the page and try again.');
                    }
                    // A status poll can answer a moment after the test has ended
                    sent = { effect, color, until: Date.now() + (effectSeconds(effect) + 2) * 1000 };
                });
        };

        // Takes no space until there is something to say, so that the rules start higher on a phone
        const showCheck = (text, color, isError) => {
            checkMessage.replaceChildren(...(text ? withLogin(text) : []));
            checkRow.hidden = !text;
            checkMessage.classList.toggle('error', Boolean(isError));
            if (color) {
                const dot = element('span', 'leddot');
                dot.style.background = color;
                checkMessage.prepend(dot);
            }
            if (text) {
                say(text);
            }
        };

        // The name of a color order, without the remark of its option
        const orderName = option => option.textContent.split(' ')[0];
        const colorWords = { R: 'red', G: 'green', B: 'blue' };
        // The user picks what the LED showed, among the colors an LED shows for red, green and blue in every color order,
        // and the order that shows them right follows from it. A dark LED leads to the GPIOs other boards have it on,
        // because a different color order can't fix that.
        const showCheckResult = () => {
            const question = 'What did the LED show, one after the other?';
            const answers = element('div', 'ledseen');
            answers.setAttribute('role', 'group');
            answers.setAttribute('aria-label', question);
            // Right first, then in the order of the color shown first
            ['RGB', 'RBG', 'GRB', 'GBR', 'BRG', 'BGR'].forEach(colors => {
                const text = [...colors].map(letter => colorWords[letter]).join(', ');
                const answer = button('', text[0].toUpperCase() + text.slice(1));
                // The colors themselves, so that they can be matched with what the LED showed at a glance
                answer.prepend(...[...colors].map(letter => {
                    const dot = element('span', 'leddot');
                    dot.style.background = '#' + checkSteps['RGB'.indexOf(letter)][0];
                    return dot;
                }));
                answer.addEventListener('click', () => {
                    const order = orderName(fields.LEDORD.selectedOptions[0]);
                    // Before the answers go away
                    checkButton.focus();
                    if (colors === 'RGB') {
                        showCheck('Color order ' + order + ' is right.' + (fields.LEDORD.value === saved.order ? '' : ' Save to keep it.'));
                        return;
                    }
                    // The LED shows each color where this order puts it, so its own order follows from what it showed
                    const native = [...order].map(letter => colors['RGB'.indexOf(letter)]).join('');
                    fields.LEDORD.value = $$('option', fields.LEDORD).find(option => orderName(option) === native).value;
                    fields.LEDORD.dispatchEvent(new Event('change', { bubbles: true }));
                    check('Color order changed to ' + native + '.');
                });
                answers.append(answer);
            });
            const link = element('a', '', 'GPIO Configuration');
            link.href = '/get?page=gpiocfg';
            const pins = (checkRow.dataset.pins || '').split(',').filter(Boolean);
            const pinList = pins.length > 1 ? pins.slice(0, -1).join(', ') + ' or ' + pins[pins.length - 1] : pins[0];
            const dark = element('p', '', 'If it stayed dark, color order can\'t fix that: the LED is probably on another GPIO' + (pins.length
                ? '. Other boards have it on GPIO ' + pinList + '. Give ' + (pins.length > 1 ? 'one of them' : 'it') + ' the LED role on '
                : ', see '));
            dark.append(link, pins.length ? ', then check again.' : '.');
            checkMessage.classList.remove('error');
            checkRow.hidden = false;
            checkMessage.replaceChildren(element('div', '', question), answers, dark);
            say(question + ' ' + dark.textContent);
            const close = button('', 'Close');
            close.addEventListener('click', () => {
                showCheck('');
                checkButton.focus();
            });
            checkMessage.append(close);
        };

        // A test or color check takes over the LED from the one before it
        let run = 0;
        let checking = false;
        let countdown = null;
        const stopCountdown = () => {
            if (countdown) {
                clearInterval(countdown.timer);
                countdown.button.textContent = 'Test';
                countdown.playing.textContent = '';
                countdown = null;
            }
        };
        let previewTimer = 0;
        const startRun = () => {
            run++;
            clearTimeout(previewTimer);
            stopCountdown();
            // A finished check stays: its question is still open, and removing it would move the rules
            if (checkMessage && (checking || checkMessage.classList.contains('error'))) {
                showCheck('');
            }
            checking = false;
            [...rows(), otherwiseRow].forEach(row => delete row.dataset.error);
            return run;
        };

        // automatic: started by an edit of the rule, and not by its button
        const test = (row, testButton, automatic) => {
            if (!(automatic ? fields.LEDBRI.checkValidity() : fields.LEDBRI.reportValidity())) {
                return;
            }
            const top = testButton.getBoundingClientRect().top;
            const id = startRun();
            const seconds = testSeconds(row);
            refresh();
            // A message that went away must not move another control under the pointer
            window.scrollBy(0, testButton.getBoundingClientRect().top - top);
            // The countdown starts when the hub has answered, because only then is the LED showing the rule. The open rule
            // says so in words too, next to its dot.
            testButton.textContent = '\u2026';
            countdown = { button: testButton, playing: $('.ledplaying', row), timer: 0 };
            // A rule as it is saved, with the saved brightness and color order, or else a look the LED doesn't keep
            const isSaved = saved !== null && saved.rules.split(';').includes(entryOf(row)) && fields.LEDBRI.value === saved.brightness && fields.LEDORD.value === saved.order;
            ledRequest($('.ledeffect', row).value, hex($('.ledcolor', row))).then(() => {
                if (id !== run) {
                    return;
                }
                let left = seconds;
                testButton.textContent = left + ' s';
                countdown.playing.textContent = isSaved ? 'on the LED now' : 'on the LED now, not saved';
                countdown.timer = setInterval(() => {
                    left--;
                    if (left > 0) {
                        testButton.textContent = left + ' s';
                    } else {
                        stopCountdown();
                    }
                }, 1000);
                say('Showing on the LED for ' + seconds + ' seconds');
            }, error => {
                if (id === run) {
                    stopCountdown();
                    row.dataset.error = (automatic ? 'Not shown on the LED: ' : 'Test failed: ') + error.message;
                    refresh();
                    say(row.dataset.error);
                }
            });
        };

        // note: what changed for this check, said while it runs
        const check = note => {
            if (!fields.LEDBRI.reportValidity()) {
                return;
            }
            const id = startRun();
            checking = true;
            const before = note ? note + ' ' : '';
            // The error of an earlier test goes away with it
            refresh();
            showCheck(before + 'Starting the check\u2026');
            const step = index => {
                if (id !== run) {
                    return;
                }
                if (index === checkSteps.length) {
                    checking = false;
                    showCheckResult();
                    return;
                }
                const [color, word, time] = checkSteps[index];
                ledRequest('solid', color).then(() => {
                    if (id === run) {
                        showCheck(before + 'The LED should be ' + word + ' now.', '#' + color);
                        setTimeout(() => step(index + 1), time);
                    }
                }, error => {
                    if (id === run) {
                        checking = false;
                        showCheck('Check failed: ' + error.message, '', true);
                    }
                });
            };
            step(0);
        };

        if (checkButton) {
            checkButton.addEventListener('click', () => check(''));
        }
        // The result of a color check is about the order that was selected
        const clearCheck = () => {
            if (checking) {
                startRun();
            } else if (checkMessage) {
                showCheck('');
            }
        };
        // The Undo of a Discard would put the old brightness and order back over these edits
        const dismissDiscardUndo = () => {
            if (undo && saveBar.contains(undo)) {
                dismissUndo();
            }
        };
        // Brightness can only be judged on the LED, so it shows there a moment after an edit, in the color the LED has
        // now. During the color check, the next step of the check shows it anyway.
        const previewBrightness = () => {
            clearTimeout(previewTimer);
            if (!canTest || checking || saving || !fields.LEDBRI.checkValidity()) {
                return;
            }
            previewTimer = setTimeout(() => {
                const id = startRun();
                refresh();
                const [, , , effect, color] = (shownEntry() || '').split(',');
                const lit = effect && effect !== 'off' && color !== '000000';
                ledRequest('solid', lit ? color : 'ffffff').then(() => {
                    if (id === run) {
                        say('Showing this brightness on the LED for 3 seconds.');
                    }
                }, () => {
                    // The line "Now showing" already says when the hub doesn't answer
                });
            }, 400);
        };
        // A look can only be judged on the LED too, so a changed effect or color plays there a moment after the edit
        const previewRule = row => {
            clearTimeout(previewTimer);
            if (!canTest || checking || saving) {
                return;
            }
            previewTimer = setTimeout(() => {
                if (row.isConnected) {
                    test(row, $('.ledtest', row), true);
                }
            }, 400);
        };
        // Under the field, like the seconds of a rule: a brightness that isn't taken, and what the LED does until then
        const brightnessHint = element('div', 'ledfieldhint');
        brightnessHint.id = newId();
        brightnessHint.hidden = true;
        fields.LEDBRI.after(brightnessHint);
        fields.LEDBRI.setAttribute('aria-describedby', brightnessHint.id);
        // always: also when the field is empty, because it has been left, or Save or a test needs it now
        const checkBrightness = always => {
            const field = fields.LEDBRI;
            const wrong = !field.validity.valid && (always || field.value !== '' || field !== document.activeElement);
            setHint(brightnessHint, wrong ? 'Choose a whole number from 1 to 100. Until then the LED keeps ' + saved.brightness + ' %.' : '');
            markWrong(field, brightnessHint, wrong);
        };
        fields.LEDBRI.addEventListener('input', () => {
            dismissDiscardUndo();
            refreshUnsaved();
            previewBrightness();
            checkBrightness(false);
        });
        fields.LEDBRI.addEventListener('blur', () => checkBrightness(true));
        fields.LEDBRI.addEventListener('invalid', () => checkBrightness(true));
        fields.LEDORD.addEventListener('change', () => {
            dismissDiscardUndo();
            refreshUnsaved();
            clearCheck();
        });

        const setOpen = (row, open) => {
            // On a phone the hints of an open rule show once their field is used: anew each time it opens
            if (row.classList.contains('open') !== open) {
                $$('.ledused', row).forEach(used => used.classList.remove('ledused'));
            }
            row.classList.toggle('open', open);
            $('.ledsum', row).setAttribute('aria-expanded', open);
        };
        // What a choice means is there once it is made or about to be, and stays until the rule closes, so that nothing
        // moves up under the finger
        ['focusin', 'change'].forEach(type => ledRules.addEventListener(type, event => {
            const used = event.target.closest('.ledfield');
            if (used) {
                used.classList.add('ledused');
            }
        }));
        // One rule at a time shows its fields, so that the list stays readable. A rule with seconds that aren't right
        // stays open with the browser's message about them, because a closed rule couldn't show it: false then.
        const openRule = row => {
            const others = $$('.ledrule.open', ledRules).filter(other => other !== row);
            const wrong = others.map(other => $('.ledtiming input', other)).find(seconds => seconds && !seconds.disabled && !seconds.validity.valid);
            if (wrong) {
                wrong.reportValidity();
            } else {
                others.forEach(other => setOpen(other, false));
            }
            if (row) {
                setOpen(row, !wrong);
            }
            return !wrong;
        };
        const closeRules = () => openRule(null);
        // Room below the Save bar while a rule closes, so that a page scrolled to its end doesn't get shorter and move the
        // pressed line away from the pointer. Below the bar it reads as the end of the page, with Save right under the
        // rules, and it goes once it is out of sight.
        const room = element('div');
        saveBar.after(room);
        window.addEventListener('scroll', () => {
            if (room.style.height && room.getBoundingClientRect().top >= window.innerHeight) {
                room.style.height = '';
            }
        }, { passive: true });
        const toggleRule = row => {
            const line = $('.ledsum', row);
            const top = line.getBoundingClientRect().top;
            const opening = !row.classList.contains('open');
            const tall = document.documentElement.scrollHeight;
            // Enough for every open rule to close, then just what keeps the page as tall as it was
            room.style.height = $$('.ledrule.open', ledRules).reduce((sum, open) => sum + open.offsetHeight, room.offsetHeight) + 'px';
            const done = opening ? openRule(row) : closeRules();
            room.style.height = Math.max(0, tall - (document.documentElement.scrollHeight - room.offsetHeight)) + 'px';
            if (!done) {
                return;
            }
            // A rule above can close: keeps the pressed line under the pointer
            window.scrollBy(0, line.getBoundingClientRect().top - top);
            if (opening) {
                row.scrollIntoView({ block: 'nearest' });
            }
        };
        // Opens the first of these rules below the open one, or else the first of them, and goes there, so that each
        // press goes on to the next
        const showNext = candidates => {
            const all = rows();
            const open = all.findIndex(row => row.classList.contains('open'));
            const present = all.filter(row => candidates.includes(row));
            const target = present.find(row => all.indexOf(row) > open) || present[0];
            if (target && openRule(target)) {
                target.scrollIntoView({ block: 'nearest' });
                $('.ledsum', target).focus({ preventScroll: true });
            }
        };
        warnedGo.addEventListener('click', () => showNext(warned));

        ledRules.addEventListener('change', event => {
            const row = event.target.closest('.ledrule');
            let text = '';
            if (row) {
                delete row.dataset.error;
                if (list.contains(row) && (event.target.matches('.ledtrigger') || event.target.matches('.ledtiming select'))) {
                    const timing = $('.ledtiming select', row);
                    const before = timing.value;
                    updateTiming(row);
                    if (timing.value !== before) {
                        text = row.dataset.timing ? momentNote : 'Timing is back to ' + timing.selectedOptions[0].textContent + '.';
                    }
                }
                // A rule that was off has no visible color yet
                if (event.target.matches('.ledeffect') && event.target.value !== 'off' && hex($('.ledcolor', row)) === '000000') {
                    $('.ledcolor', row).value = '#ffffff';
                }
            }
            const top = event.target.getBoundingClientRect().top;
            dismissListUndo();
            refresh();
            say(text, row);
            // An Undo can go from above: keeps the field under the pointer
            window.scrollBy(0, event.target.getBoundingClientRect().top - top);
        });
        ledRules.addEventListener('input', event => {
            if (event.target.matches('.ledtiming input')) {
                // The message about the value before would keep the field wrong
                event.target.setCustomValidity('');
                if (event.target.validity.valid) {
                    event.target.dataset.good = event.target.value;
                }
            }
            dismissListUndo();
            refresh();
            if (event.target.matches('.ledeffect, .ledcolor')) {
                previewRule(event.target.closest('.ledrule'));
            }
        });
        // Seconds left empty look wrong once the focus has left the field
        ledRules.addEventListener('focusout', event => {
            if (event.target.matches('.ledtiming input') && event.target.value === '') {
                setTimeout(refresh);
            }
        });

        ledRules.addEventListener('click', event => {
            const row = event.target.closest('.ledrule');
            if (event.target.closest('.ledsum')) {
                toggleRule(row);
            } else if (event.target.closest('.ledremove')) {
                removeRule(row);
            } else if (event.target.closest('.ledmove')) {
                moveByButton(row, event.target.closest('.ledmove'));
            } else if (event.target.closest('.ledtest')) {
                test(row, event.target.closest('.ledtest'));
            } else if (event.target.closest('.lednotefix')) {
                moveByFix(row, event.target.closest('.lednotefix'));
            }
        });

        addButton.addEventListener('click', () => {
            // An open rule with seconds that aren't right says so first
            if (rows().length >= max || !closeRules()) {
                return;
            }
            dismissListUndo();
            // Starts on a state no rule covers yet, from the lock states on, and on a device that is enabled
            const options = $$('option', triggerTemplate.content);
            const first = options.findIndex(option => option.value === 'locked');
            const order = options.slice(first).concat(options.slice(0, first));
            const used = new Set(rows().map(row => $('.ledtrigger', row).value));
            const trigger = (order.find(option => !used.has(option.value) && !option.dataset.off)
                || order.find(option => !used.has(option.value)) || order[0]).value;
            // In a color that isn't used yet, so that the new rule doesn't look like Otherwise
            const colors = new Set([...rows(), otherwiseRow].map(colorOf));
            const color = palette.find(candidate => !colors.has(candidate)) || 'ffffff';
            const row = ruleRow({ trigger, timing: eventTriggers.has(trigger) ? 'for' : 'while', seconds: 0, effect: 'solid', color });
            list.append(row);
            // A full list has no room to put a removed rule back
            if (rows().length >= max) {
                dismissUndo();
            }
            openRule(row);
            refresh();
            $('.ledtrigger', row).focus({ preventScroll: true });
            // Which reads the warning of the new rule, and a rule at the end changes no other
            hearAll();
            // At the end of the list, which can be far below the button, clear of the Save bar
            row.scrollIntoView({ block: 'nearest' });
            // And of the sticky header, with its Test button
            const header = $('header.top');
            const covered = (header ? header.getBoundingClientRect().bottom : 0) + 8 - row.getBoundingClientRect().top;
            if (covered > 0) {
                window.scrollBy(0, -covered);
            }
        });

        let presetNotice = null;
        let presetBefore = null;
        $$('[data-led-preset]').forEach(preset => preset.addEventListener('click', () => {
            // Without rules for a device that isn't enabled, because they could never show
            const target = ledRules.dataset[preset.dataset.ledPreset].split(';').filter(entry => !disabledTriggers.has(entry.split(',')[0])).join(';');
            // Checked before anything changes, so that the second click of a double click keeps the Undo
            if (value.value === target) {
                if (!undo) {
                    undo = element('div', 'ledundo', 'These rules are already on the page.');
                    hintBlock.after(undo);
                }
                say('The page already has these rules.');
                return;
            }
            // When comparing presets, Undo goes back to the list from before the first one
            const before = undo && undo === presetNotice ? presetBefore : value.value;
            load(target);
            const text = 'Rules replaced with "' + preset.textContent + '".';
            const notice = undoNotice(text, () => {
                load(before);
                preset.focus();
            }, 'div');
            presetNotice = notice;
            presetBefore = before;
            hintBlock.after(notice);
            $('button', notice).focus();
            say(text);
        }));

        // Returns false when the rule is already there, so that a move without effect keeps the Undo
        const moveRule = (row, key, focusTarget) => {
            const all = rows();
            const from = all.indexOf(row);
            const to = key === 'Home' ? 0 : key === 'End' ? all.length - 1 : from + (key === 'ArrowUp' ? -1 : 1);
            if (to < 0 || to >= all.length || to === from) {
                return false;
            }
            dismissListUndo();
            // Moving the neighbor keeps the focus, and the screen reader, on this row
            if (key === 'ArrowUp') {
                list.insertBefore(all[from - 1], row.nextElementSibling);
            } else if (key === 'ArrowDown') {
                list.insertBefore(all[from + 1], row);
            } else if (key === 'Home') {
                list.prepend(row);
            } else {
                list.append(row);
            }
            (focusTarget || $('.ledhandle', row)).focus();
            refresh();
            return true;
        };
        const moveByButton = (row, pressed) => {
            const top = pressed.getBoundingClientRect().top;
            if (moveRule(row, pressed.dataset.key, pressed)) {
                // Keeps the button under the finger, so that the next tap moves the same rule again
                window.scrollBy(0, pressed.getBoundingClientRect().top - top);
                // At the end of the list this button is disabled and can't keep the focus
                if (pressed.disabled) {
                    ($('.ledmove:not(:disabled)', row) || $('.ledhandle', row)).focus();
                }
                say('Moved to position ' + (rows().indexOf(row) + 1) + ' of ' + rows().length + '.', row);
            }
        };

        // Puts the rule of a note above or below the rule its button names
        const moveByFix = (row, pressed) => {
            const before = rows();
            const target = before[pressed.dataset.to - 1];
            const up = pressed.dataset.side === 'before';
            const text = '"' + $('.ledtrigger', row).selectedOptions[0].textContent + '" moved ' + (up ? 'up.' : 'down.');
            const notice = undoNotice(text, () => {
                list.replaceChildren(...before);
                undo = null;
                refresh();
                $('.ledhandle', row).focus();
                say('', row);
            });
            notice.dataset.order = '1';
            row.before(notice);
            if (up) {
                target.before(row);
            } else {
                target.after(row);
            }
            refresh();
            // Shows where the rule went, and the Undo where it was if both fit on the screen
            const undoButton = $('button', notice);
            undoButton.focus({ preventScroll: true });
            undoButton.scrollIntoView({ block: 'nearest' });
            $('.ledsum', row).scrollIntoView({ block: 'nearest' });
            say(text.slice(0, -1) + ' to position ' + (rows().indexOf(row) + 1) + ' of ' + rows().length + '.', row);
        };

        let moveTimer = 0;
        list.addEventListener('keydown', event => {
            const handle = event.target.closest('.ledhandle');
            if (!handle || !['ArrowUp', 'ArrowDown', 'Home', 'End'].includes(event.key)) {
                return;
            }
            event.preventDefault();
            // The hint that names these keys has done its job
            list.classList.add('ledkeysknown');
            const row = handle.closest('.ledrule');
            if (moveRule(row, event.key)) {
                // Focus alone doesn't scroll to an element that already has it
                row.scrollIntoView({ block: 'nearest' });
                // Once the keys rest, so that a rule passing by doesn't fill the announcement with warnings
                clearTimeout(moveTimer);
                moveTimer = setTimeout(() => {
                    if (list.contains(row)) {
                        say('Moved to position ' + (rows().indexOf(row) + 1) + ' of ' + rows().length + '.', row);
                    }
                }, 600);
            }
        });

        // Pointer events cover mouse, touch and pen. Dragging moves the neighbors, so the row itself stays in the DOM
        // and keeps the pointer and the focus.
        let dragging = false;
        const middle = el => {
            const box = el.getBoundingClientRect();
            return box.top + box.height / 2;
        };
        list.addEventListener('pointerdown', event => {
            const handle = event.target.closest('.ledhandle');
            if (!handle || event.button !== 0 || dragging) {
                return;
            }
            // On a touch screen only the dots drag, so that a swipe next to them scrolls the page
            if (event.pointerType !== 'mouse' && !event.target.closest('.ledgrip')) {
                return;
            }
            event.preventDefault();
            handle.focus();
            dragging = true;
            const row = handle.closest('.ledrule');
            // What Escape goes back to. An Undo above or in the list stays until the drop, so that the list doesn't move
            // under the pointer.
            const start = [...list.children];
            const from = rows().indexOf(row);
            const pointer = event.pointerId;
            let reordered = false;
            let y = event.clientY;
            let direction = 0;
            let frame = 0;
            row.classList.add('dragging');
            const place = () => {
                let moved = false;
                while (row.previousElementSibling && y < middle(row.previousElementSibling)) {
                    list.insertBefore(row.previousElementSibling, row.nextElementSibling);
                    moved = true;
                }
                while (row.nextElementSibling && y > middle(row.nextElementSibling)) {
                    list.insertBefore(row.nextElementSibling, row);
                    moved = true;
                }
                if (moved) {
                    reordered = true;
                    refresh();
                }
            };
            // Keeps scrolling while the pointer rests near the sticky header or the save bar
            const scroll = () => {
                window.scrollBy(0, direction * 8);
                place();
                frame = direction ? requestAnimationFrame(scroll) : 0;
            };
            const move = moveEvent => {
                if (moveEvent.pointerId !== pointer) {
                    return;
                }
                y = moveEvent.clientY;
                place();
                const header = $('header.top');
                const top = (header ? header.getBoundingClientRect().bottom : 0) + 40;
                const bottom = Math.min(window.innerHeight, saveBar.getBoundingClientRect().top) - 40;
                direction = y < top ? -1 : y > bottom ? 1 : 0;
                if (direction && !frame) {
                    frame = requestAnimationFrame(scroll);
                }
            };
            const end = endEvent => {
                if (endEvent.pointerId !== undefined && endEvent.pointerId !== pointer) {
                    return;
                }
                dragging = false;
                direction = 0;
                cancelAnimationFrame(frame);
                row.classList.remove('dragging');
                window.removeEventListener('pointermove', move);
                window.removeEventListener('pointerup', end);
                window.removeEventListener('pointercancel', end);
                window.removeEventListener('blur', end);
                window.removeEventListener('keydown', cancel);
                const moved = rows().indexOf(row) !== from;
                // A new order ends the Undo of a preset or a move: keeps the rule where it was dropped
                const top = row.getBoundingClientRect().top;
                if (moved) {
                    dismissListUndo();
                }
                refresh();
                window.scrollBy(0, row.getBoundingClientRect().top - top);
                if (moved) {
                    say('Moved to position ' + (rows().indexOf(row) + 1) + ' of ' + rows().length + '.', row);
                }
            };
            // Escape puts the rule back where the drag started. The others move, so that it keeps the focus.
            const cancel = keyEvent => {
                if (keyEvent.key === 'Escape') {
                    keyEvent.preventDefault();
                    if (reordered) {
                        const back = start.filter(child => child.parentNode === list);
                        const at = back.indexOf(row);
                        back.slice(0, at).forEach(child => list.insertBefore(child, row));
                        back.slice(at + 1).forEach(child => list.append(child));
                    }
                    end({});
                }
            };
            window.addEventListener('pointermove', move);
            window.addEventListener('pointerup', end);
            window.addEventListener('pointercancel', end);
            window.addEventListener('blur', end);
            window.addEventListener('keydown', cancel);
        });

        // Enter in the seconds of a rule closes the rule, like its line does, instead of saving a rule that is still being
        // edited, and Escape anywhere in an open rule closes it too. Seconds that aren't right keep it open with their
        // message. In Brightness, Enter saves, as on the other pages.
        form.addEventListener('keydown', event => {
            const row = event.target.closest('.ledrule');
            // Escape on a handle also puts away the hint about its keys, which covers a little of the next rule
            if (event.key === 'Escape' && event.target.matches('.ledhandle')) {
                list.classList.add('ledkeysknown');
            }
            const enter = event.key === 'Enter' && event.target.matches('input[type=text], input[type=number]');
            // While a rule is dragged, Escape puts it back instead
            const escape = event.key === 'Escape' && !dragging && row && row.classList.contains('open');
            if (!row || !enter && !escape) {
                return;
            }
            event.preventDefault();
            if (row.classList.contains('open')) {
                toggleRule(row);
            }
            if (!row.classList.contains('open')) {
                $('.ledsum', row).focus();
            }
        });
        let saveTimer = 0;
        // What this page has stored for its browser tab: the edits a Save sent ('save'), edits that weren't saved when the
        // page was hidden or left ('kept'), or nothing
        let stored = '';
        const storeDraft = kept => {
            // The open rule and the place on the page come back with the edits, so that the next edit or test is where it was
            const all = [...rows(), otherwiseRow];
            const open = all.findIndex(row => row.classList.contains('open'));
            const place = { open, top: open < 0 ? 0 : all[open].getBoundingClientRect().top, y: window.scrollY };
            try {
                sessionStorage.setItem('ledDraft', JSON.stringify(Object.assign(pageState(), place, kept ? { kept } : {})));
                stored = kept ? 'kept' : 'save';
            } catch (error) {
                // Without storage, the Back button still shows the edits of a Save
            }
        };
        const dropDraft = () => {
            stored = '';
            try {
                sessionStorage.removeItem('ledDraft');
            } catch (error) {
                // Nothing was stored then
            }
        };
        const showSaving = busy => {
            saving = busy;
            saveButton.value = busy ? 'Saving\u2026' : 'Save';
            // The request is on its way: discarding now wouldn't stop the hub from storing it
            discardButton.disabled = busy;
        };
        form.addEventListener('submit', () => {
            serialize();
            saving = true;
            clearTimeout(previewTimer);
            const sent = pageState();
            failed = null;
            refreshUnsaved();
            // After the browser has read the form, so that the request stays the same
            setTimeout(() => showSaving(true));
            // A request that never ends doesn't leave the page, and Save has to work again then
            clearTimeout(saveTimer);
            saveTimer = setTimeout(() => {
                showSaving(false);
                failed = { state: sent, text: 'No answer from Nuki Hub', retry: true };
                refreshUnsaved();
                say('Nuki Hub doesn\'t answer. Your edits are still here, try Save again.');
            }, 15000);
            // Comes back if this Save doesn't reach the hub, for example after a lost connection or an expired login, and
            // tells the page that comes after it whether it did
            storeDraft(null);
        });
        window.addEventListener('beforeunload', event => {
            if (!saving && isUnsaved()) {
                event.preventDefault();
                event.returnValue = '';
            }
        });
        // A phone can close this page while another app is in front and load it again when you come back, without asking
        // first. Edits that aren't saved are kept for this browser tab while the page is hidden or left, and come back with
        // it; while it is shown, it has them itself.
        const keepDraft = () => {
            // A Save on its way, or one that got no answer for these edits, has stored them already, and the page after it
            // can still tell whether it reached the hub
            if (saving || stored === 'save' && failed && sameState(pageState(), failed.state)) {
                return;
            }
            if (isUnsaved()) {
                // With the settings they were made on, so that they don't come back onto others saved meanwhile
                storeDraft(saved);
            } else if (stored === 'kept') {
                dropDraft();
            }
        };
        window.addEventListener('pagehide', keepDraft);
        document.addEventListener('visibilitychange', () => {
            if (document.visibilityState === 'hidden') {
                keepDraft();
            } else if (stored === 'kept') {
                dropDraft();
            }
        });
        // Back from a Save that failed shows this page again as it was
        window.addEventListener('pageshow', event => {
            if (event.persisted) {
                clearTimeout(saveTimer);
                showSaving(false);
                dropDraft();
            }
        });

        load(value.value);
        // From the served values, because a browser can restore the edited ones after a reload
        const savedOrder = $$('option', fields.LEDORD).find(option => option.defaultSelected) || fields.LEDORD.options[0];
        saved = { rules: value.value, brightness: fields.LEDBRI.defaultValue, order: savedOrder.value };
        refreshUnsaved();
        document.addEventListener('hubstatus', event => {
            const status = event.detail;
            const fresh = Boolean(status) && status !== 'login';
            if (staleNote) {
                const wasFresh = staleNote.hidden;
                staleNote.hidden = fresh;
                if (!fresh) {
                    const text = status === 'login' ? ' (not updating). ' + expiredText() : ' (not updating, Nuki Hub doesn\'t answer)';
                    // Once for each change, and not with every poll
                    if (wasFresh || staleNote.dataset.text !== text) {
                        say('Now showing' + text);
                    }
                    // Kept while it says the same, so that its link keeps the focus
                    if (staleNote.dataset.text !== text) {
                        staleNote.dataset.text = text;
                        staleNote.replaceChildren(...withLogin(text));
                    }
                }
            }
            shownRule = fresh && Number.isInteger(status.ledshownrule) ? status.ledshownrule : -1;
            // "Test, " and the name of the effect
            const shownText = fresh && typeof status.ledshown === 'string' ? status.ledshown : '';
            testShown = shownRule === -1 && shownText.startsWith('Test, ') ? shownText.slice(6) : '';
            markShown();
        });

        const applyState = state => {
            const orderChanged = fields.LEDORD.value !== state.order;
            fields.LEDBRI.value = state.brightness;
            checkBrightness(false);
            fields.LEDORD.value = state.order;
            load(state.rules);
            if (orderChanged) {
                clearCheck();
            }
        };
        discardButton.addEventListener('click', () => {
            const edits = pageState();
            // A Save that never finished left these edits for the next visit
            dropDraft();
            applyState(saved);
            const notice = undoNotice('Edits discarded.', () => {
                applyState(edits);
                discardButton.focus();
            }, 'span');
            saveBar.append(notice);
            $('button', notice).focus();
            say('Edits discarded.');
        });

        // What the last Save sent, or edits that weren't saved when the page was hidden or left, kept for this browser tab:
        // the edits come back if they never reached the hub
        let draft = null;
        try {
            draft = JSON.parse(sessionStorage.getItem('ledDraft'));
            sessionStorage.removeItem('ledDraft');
        } catch (error) {
            draft = null;
        }
        // Edits that weren't saved only come back onto the settings they were made on
        if (draft && draft.kept && (!sameState(draft.kept, saved) || sameState(draft, saved))) {
            draft = null;
        }
        if (draft && typeof draft.rules === 'string') {
            if (draft.kept) {
                failed = { state: draft, text: 'Edits kept, not saved' };
                applyState(draft);
                say('Your edits are back in the form, not saved yet.');
            } else if (!sameState(draft, saved)) {
                failed = { state: draft, text: 'Save failed, edits kept', retry: true };
                applyState(draft);
                say('Your last save didn\'t reach Nuki Hub, so your edits are back in the form.');
            } else {
                // A moment after the page has loaded, so that a screen reader hears it
                setTimeout(() => {
                    justSaved = true;
                    refreshUnsaved();
                    setTimeout(() => {
                        justSaved = false;
                        refreshUnsaved();
                    }, 6000);
                }, 500);
            }
            const row = [...rows(), otherwiseRow][draft.open];
            if (row) {
                openRule(row);
            }
            // The browser would put back where a reload left the page, which is not where the rule is now
            if ('scrollRestoration' in history) {
                history.scrollRestoration = 'manual';
            }
            window.scrollTo(0, Number(draft.y) || 0);
            // Where the open rule was on the screen, if the page above it has changed
            if (row && Number.isFinite(draft.top)) {
                window.scrollBy(0, row.getBoundingClientRect().top - draft.top);
            }
        }
    }
})();
