#
# (C) Copyright 2011-2012 Sergey A. Babkin.
# This file is a part of Triceps.
# See the file COPYRIGHT for the copyright notice and license information
#
# Perl methods for the Table class.

package Triceps::Table;
use Carp;

# create a row with specified fields and find it, thus 
# making more convenient to search by key fields
sub findBy # (self, fieldName => fieldValue, ...)
{
	my $self = shift;
	my $row = $self->getRowType()->makeRowHash(@_) or Carp::confess "$!";
	return $self->find($row);
}

# create a row with specified fields and find it in an expicit index, thus 
# making more convenient to search by key fields
sub findIdxBy # (self, idxType, fieldName => fieldValue, ...)
{
	my $self = shift;
	my $idx = shift;
	my $row = $self->getRowType()->makeRowHash(@_) or Carp::confess "$!";
	return $self->findIdx($idx, $row);
}

1;