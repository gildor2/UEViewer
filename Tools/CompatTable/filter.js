function filter(phrase, _id, _id2) {
	// split phrase to words
	var hasFilter = (phrase.value != "");
	var words = phrase.value.toLowerCase().split(" ");
	// remove all empty items
	for (var i = words.length-1; i >= 0; i--)
		if (words[i] == "")
			words.splice(i, 1);
	// prepare regexp
///	var reg = new RegExp("(" + words.join("|") + ")", "i");
///	DEBUG("WORDS: " + words + " -> [" + reg + "]" + " regexp: [" + reg.source + "]");
	// get table
	var table = document.getElementById(_id);
	// iterate all rows starting from 1 (keep table header)
	var lastInfo = -1;	// current info line
	var count = 0;		// number of games found
	for (var r = 1; r < table.rows.length; r++) {
		var text = table.rows[r].innerHTML;
		// search for special tags
		// note: IE can change case and remove quotes!
		var collapsed = (text.search(/colspan=/i) >= 0);
		var isEngine  = (text.search(/class=(")?engine/i) >= 0);
		var isYear    = (text.search(/class=(")?year/i) >= 0);
		// remove HTML tags, special spaces and newlines
		text = text.replace(/(<[^>]+>|&nbsp;|\r|\n)/g," ");
		// squeeze spaces
		text = text.replace(/\s+/g, " ").toLowerCase();
		var hide = false;
		if (text.match(/^\s+$/)) {
			// empty line - keep when no active filter only
			hide = hasFilter;
//			DEBUG("["+text+"]... empty, hide="+hide);
		} else if (isEngine) {
			// nothing
//			DEBUG("["+text+"] -- engine");
		} else if (isYear) {
			// year lines
			hide = false;
//			DEBUG("["+text+"]... year");
			// remove previous info if one
			if (lastInfo >= 0 && hasFilter) {
//				DEBUG("remove row " + lastInfo + "(" + table.rows[lastInfo].innerHTML + ")");
				table.rows[lastInfo].style.display = 'none';
			}
			lastInfo = r;
		} else if (collapsed) {
			// other information, do not remove
//			DEBUG("[" + text + "] -- other info");
		} else {
//			DEBUG("["+text+"]... GAME");
			// it's a game
			for (var i = 0; i < words.length; i++) {
				if (text.indexOf(words[i]) >= 0) {
///				if (reg.test(text)) {
//					DEBUG("... matched " + text);
					hide = false;
				} else {
					hide = true;
					break;
				}
			}
			if (!hide) {
				count++;
				lastInfo = -1;		// do not remove info
			}
		}
		if (hide) {
			table.rows[r].style.display = 'none';
		} else {
			table.rows[r].style.display = '';
		}
	}
	// remove last info if one
	if (lastInfo >= 0 && hasFilter) {
//		DEBUG("remove row " + lastInfo + "(" + table.rows[lastInfo].innerHTML + ")");
		table.rows[lastInfo].style.display = 'none';
	}
	// show game count
	var filter_info = document.getElementById(_id2);
	if (hasFilter) {
		filter_info.innerText = count + ' game(s)';
	} else {
		filter_info.innerText = '';
	}
}

function initFilter() {
	var param = window.location.search.substring(1);
	var inp = document.getElementById("filt_input");
	if (param != "") {
		// facebook reference id
		param = param.replace(/\.?\-?\&fbclid=.*/, "");
		// replace special characters
		param = unescape(param);
		param = param.replace(/\+/g, " ");
		// set input and execute filter
		inp.value = param;
		filter(inp, 'compat_table', 'filter_info');
	}
	document.getElementById("filter_form").addEventListener("submit", function(e) { e.preventDefault(); }, false);
}

window.addEventListener("load", initFilter, false);
