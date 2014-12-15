#!/usr/bin/perl

# 
# Copyright (C) 2014 Richard Burke
# Based on unicode.vim by Bram Moolenaar
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#

use strict;
use warnings;

use File::Basename;
use LWP::Simple;

main();

sub main {
    my $script_name = chdir_to_script_dir();
    my $cfg = get_config();
    my %uni_data;

    download_unicode_files($cfg);
    parse_files($cfg, \%uni_data);
    write_uni_data($cfg, \%uni_data, $script_name, 'unicode.c');
}

sub chdir_to_script_dir {
    my ($script_name, $script_dir) = fileparse(__FILE__);
    chdir($script_dir) or die "Can't chdir to $script_dir: $!";
    return $script_name;
}

sub get_config {
    my %cfg = (
        combining => {
            url => 'http://www.unicode.org/Public/UNIDATA/UnicodeData.txt',
            filename => 'UnicodeData.txt',
            field_num => 15,
            parse_line_func => \&parse_line_combining,
            write_output_func => \&write_1d_array
        },
        uppercase => {
            url => 'http://www.unicode.org/Public/UNIDATA/UnicodeData.txt',
            filename => 'UnicodeData.txt',
            field_num => 15,
            parse_line_func => \&parse_line_uppercase,
            write_output_func => \&write_2d_array
        },
        lowercase => {
            url => 'http://www.unicode.org/Public/UNIDATA/UnicodeData.txt',
            filename => 'UnicodeData.txt',
            field_num => 15,
            parse_line_func => \&parse_line_lowercase,
            write_output_func => \&write_2d_array
        },
        case_folding => {
            url => 'http://www.unicode.org/Public/UNIDATA/CaseFolding.txt',
            filename => 'CaseFolding.txt',
            field_num => 4,
            parse_line_func => \&parse_line_case_folding,
            write_output_func => \&write_2d_array
        },
        double_width => {
            url => 'http://www.unicode.org/Public/UNIDATA/EastAsianWidth.txt',
            filename => 'EastAsianWidth.txt',
            field_num => 2,
            parse_line_func => \&parse_double_width,
            write_output_func => \&write_2d_array
        }
    );

    return \%cfg;
}

sub download_unicode_files {
    my $cfg = shift;
    my %url_filename = map { $cfg->{$_}->{url} => $cfg->{$_}->{filename} } keys %$cfg;

    for my $url (keys %url_filename) {
        print "Downloading $url_filename{$url}\n";
        download_file($url, $url_filename{$url}); 
    }
}

sub parse_files {
    my ($cfg, $uni_data) = @_;
    my %file_parse_cfgs;
    my %file_field_num;
    my $filename;

    for my $cfg_type (keys %$cfg) {
        $uni_data->{$cfg_type} = [];
        $filename = $cfg->{$cfg_type}->{filename};

        if (!exists($file_parse_cfgs{$filename})) {
            $file_parse_cfgs{$filename} = [];
            $file_field_num{$filename} = $cfg->{$cfg_type}->{field_num};
        }

        push(@{$file_parse_cfgs{$filename}}, {
            cfg_type => $cfg_type,
            parse_line_func => $cfg->{$cfg_type}->{parse_line_func}
        });
    }

    for my $file (keys %file_parse_cfgs) {
        print "Parsing $file\n";
        parse_file($file, $file_field_num{$file}, $file_parse_cfgs{$file}, $uni_data);
    }
}

sub write_uni_data {
    my ($cfg, $uni_data, $script_name, $output_file) = @_;
    
    open(my $fh, '>', $output_file) 
        or die "Unable to open file $output_file for writing: $!";

    print "Writing $output_file\n";

    print $fh "/* Generated by $script_name */\n";

    for my $cfg_type (keys %$cfg) {
        $cfg->{$cfg_type}->{write_output_func}($fh, $cfg_type, $uni_data->{$cfg_type});
    }

    close($fh);
}

sub download_file {
    my ($url, $filename) = @_;

    if (-e $filename) {
        unlink($filename) or die "Unable to unlink $filename: $!"
    }

    my $return_code = getstore($url, $filename);

    if (is_error($return_code)) {
        die "Unable to download file at $url. Return code $return_code";
    }

    if (! -f $filename)  {
        die "File $filename downloaded from $url doesn't exist";
    }
}

sub parse_file {
    my ($filename, $field_num, $parse_cfgs, $uni_data) = @_;

    open(my $fh, '<:encoding(UTF-8)', $filename) 
        or die "Unable to open file $filename for reading: $!";

    my $line_num = 0;
    my @fields;

    while (my $line = <$fh>) {
        $line_num++;

        if ($line =~ /^(\s*#.*|)$/) {
            next;
        }

        @fields = split(/;/, $line); 

        if (scalar(@fields) != $field_num) {
            die "$filename:$line_num has " . scalar(@fields) .
                " fields, expected $field_num";
        }

        s/^\s+|(\s+|\s*#.*)$//g for @fields;

        for my $parse_cfg (@$parse_cfgs) {
            $parse_cfg->{parse_line_func}(\@fields, $parse_cfg->{cfg_type}, $uni_data);
        }
    }

    close($fh);
}

sub parse_line_combining {
    my ($fields, $cfg_type, $uni_data) = @_;

    if ($fields->[2] =~ /M(c|e|n)/) {
        push(@{$uni_data->{$cfg_type}}, "0x$fields->[0]");
    }
}

sub parse_line_uppercase {
    my ($fields, $cfg_type, $uni_data) = @_;

    if ($fields->[12]) {
       push(@{$uni_data->{$cfg_type}}, ["0x$fields->[0]", "0x$fields->[12]"]); 
    }
}

sub parse_line_lowercase {
    my ($fields, $cfg_type, $uni_data) = @_;

    if ($fields->[13]) {
       push(@{$uni_data->{$cfg_type}}, ["0x$fields->[0]", "0x$fields->[13]"]); 
    }
}

sub parse_line_case_folding {
    my ($fields, $cfg_type, $uni_data) = @_;

    if ($fields->[1] eq 'C' or $fields->[1] eq 'S') {
        push(@{$uni_data->{$cfg_type}}, ["0x$fields->[0]", "0x$fields->[2]"]);
    }
}

sub parse_double_width {
    my ($fields, $cfg_type, $uni_data) = @_;
    
    if ($fields->[1] eq 'W' or $fields->[1] eq 'F') {
        if ($fields->[0] =~ /([A-Z0-9]+)\.\.([A-Z0-9]+)/i) {
            push(@{$uni_data->{$cfg_type}}, ["0x$1", "0x$2"]);
        } else {
            push(@{$uni_data->{$cfg_type}}, ["0x$fields->[0]", "0x$fields->[0]"]);
        }
    }
}

sub write_1d_array {
    my ($fh, $cfg_type, $data) = @_;

    print $fh "\nstatic unsigned int ${cfg_type}[] = {\n    ";
    print $fh join(",\n    ", @$data);
    print $fh "\n};\n";
}

sub write_2d_array {
    my ($fh, $cfg_type, $data) = @_;
    my @values = map { '{ ' . join(', ', @$_) . ' }' } @$data;

    print $fh "\nstatic unsigned int ${cfg_type}[][2] = {\n    ";
    print $fh join(",\n    ", @values);
    print $fh "\n};\n";
}
