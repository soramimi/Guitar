#!/usr/bin/env ruby

dirs = [
  "_bin",
  "_build*",
  "_release",
  "build"
]

require 'fileutils'

Dir.chdir("#{__dir__}/..")

dirs.each do |pattern|
  Dir.glob(pattern).each do |path|
    if File.exist?(path)
      FileUtils.rm_r(path, force: true)
    end
  end
end

