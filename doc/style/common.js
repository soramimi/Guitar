{
	var e_breadcrumb = document.getElementById("breadcrumb");
	if  (e_breadcrumb) {
		var url = escape(location.href)
		req_breadcrumb = new XMLHttpRequest();
		req_breadcrumb.open("GET", "/breadcrumb.cgi?path=" + url);
		req_breadcrumb.onreadystatechange = function() {
			if (req_breadcrumb.readyState == 4 && req_breadcrumb.status == 200) {
				var s = req_breadcrumb.responseText;
				if (s == "") {
					s = "&nbsp;"
				}
				e_breadcrumb.innerHTML = s;
			}
		}
		req_breadcrumb.send();
	}
}

{
	var e_copyright = document.getElementById("copyright");
	if (e_copyright) {
		req_copyright = new XMLHttpRequest();
		req_copyright.open("GET", "/copyright.txt");
		req_copyright.onreadystatechange = function() {
			if (req_copyright.readyState == 4 && req_copyright.status == 200) {
				e_copyright.innerHTML = req_copyright.responseText;
			}
		}
		req_copyright.send();
	}
}
