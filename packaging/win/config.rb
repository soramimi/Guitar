$script_dir = __dir__
$project_root = "#{$script_dir}/../.."

$qt = "6.10.0/msvc2022_64"

$server_address = "10.168.0.5"
$destination_path = "#{$server_address}:/Public/pub/nightlybuild"

$current_branch = `git symbolic-ref --short HEAD`.strip

$suffix = ENV["SUFFIX"]
if $suffix == nil
	$suffix = $current_branch
	if $suffix == "main" or $suffix == "master" then
		$suffix = ""
	end
end
