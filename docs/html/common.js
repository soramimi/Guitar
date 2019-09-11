
{
	var e_copyright = document.getElementById("copyright");
	if (e_copyright) {
		req_copyright = new XMLHttpRequest();
		req_copyright.open("GET", "copyright.txt");
		req_copyright.onreadystatechange = function() {
			if (req_copyright.readyState == 4 && req_copyright.status == 200) {
				e_copyright.innerHTML = req_copyright.responseText;
			}
		}
		req_copyright.send();
	}
}
