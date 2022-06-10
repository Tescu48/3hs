<!DOCTYPE html>
<html>
	<head>
		<meta charset="utf-8"/>
		<title>hLink</title>
	</head>
	<body>
		[[if is-success?()]]
			<p>Launched title [title-name] (with title id [title-id]) on the 3ds.</p>
			<p>hLink is not functional anymore.</p>
		[[else]]
			<p>An error occured: [error-message].</p>
			<a href="/index.html">Back to home</a>
		[[end]]
	</body>
</html>
