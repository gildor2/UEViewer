function filter(phrase, _id) {
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
	var lastInfo = -1;
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
		text = text.replace(/\s+/g, " ");
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
			// game
			for (var i = 0; i < words.length; i++) {
				if (text.toLowerCase().indexOf(words[i]) >= 0) {
///				if (reg.test(text)) {
//					DEBUG("... matched " + text);
					hide = false;
				} else {
					hide = true;
					break;
				}
			}
			if (!hide) lastInfo = -1;		// do not remove info
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
}
