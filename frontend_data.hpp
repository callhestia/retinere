#pragma once
const char* html_content = R"HTML(
<!DOCTYPE html>
<html lang="en">
	<head>
		<meta charset="utf-8" />
		<link rel="icon" href="/favicon.png" />
		<meta name="viewport" content="width=device-width, initial-scale=1" />
		<script src="https://cdn.tailwindcss.com"></script>
		<link href="/_app/immutable/entry/start.XTHDquo2.js" rel="modulepreload">
		<link href="/_app/immutable/chunks/DrRfCcsv.js" rel="modulepreload">
		<link href="/_app/immutable/chunks/CxFP86L7.js" rel="modulepreload">
		<link href="/_app/immutable/entry/app.D6v589ba.js" rel="modulepreload">
		<link href="/_app/immutable/chunks/DYl5dUZ5.js" rel="modulepreload">
		<link href="/_app/immutable/chunks/xihTtKlq.js" rel="modulepreload">
		<link href="/_app/immutable/nodes/0.h6EG8Rhh.js" rel="modulepreload">
		
		<link href="/_app/immutable/assets/0.x1XGuNl0.css" rel="stylesheet">
	</head>
	<body data-sveltekit-preload-data="hover" class="bg-[#121212]">
		<div style="display: contents">
			<script>
				{
					__sveltekit_dmrymx = {
						base: ""
					};

					const element = document.currentScript.parentElement;

					Promise.all([
						import("/_app/immutable/entry/start.XTHDquo2.js"),
						import("/_app/immutable/entry/app.D6v589ba.js")
					]).then(([kit, app]) => {
						kit.start(app, element);
					});
				}
			</script>
		</div>
	</body>
</html>)HTML";
