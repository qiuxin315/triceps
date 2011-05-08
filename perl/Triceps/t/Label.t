#
# This file is a part of Triceps.
# See the file COPYRIGHT for the copyright notice and license information
#
# The test for Label.

# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl Triceps.t'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use Test;
BEGIN { plan tests => 26 };
use Triceps;
ok(1); # If we made it this far, we're ok.

#########################

# Insert your test code below, the Test::More module is use()ed here so read
# its man page ( perldoc Test::More ) for help writing this test script.


######################### preparations (originating from Table.t)  #############################

$u1 = Triceps::Unit->new("u1");
ok(ref $u1, "Triceps::Unit");

@def1 = (
	a => "uint8",
	b => "int32",
	c => "int64",
	d => "float64",
	e => "string",
);
$rt1 = Triceps::RowType->new( # used later
	@def1
);
ok(ref $rt1, "Triceps::RowType");

$it1 = Triceps::IndexType->newHashed(key => [ "b", "c" ])
	->addNested("fifo", Triceps::IndexType->newFifo()
	);
ok(ref $it1, "Triceps::IndexType");

$tt1 = Triceps::TableType->new($rt1)
	->addIndex("grouping", $it1);
ok(ref $tt1, "Triceps::TableType");

$res = $tt1->initialize();
ok($res, 1);
#print STDERR "$!" . "\n";

$t1 = $u1->makeTable($tt1, "SM_SCHEDULE", "tab1");
ok(ref $t1, "Triceps::Table");

$lb = $t1->getOutputLabel();
ok(ref $lb, "Triceps::Label");

########################## label g/setters #################################################

$rt2 = $lb->getType();
ok(ref $rt2, "Triceps::RowType");
ok($rt1->same($rt2));

$v = $lb->getName();
ok($v, "tab1.out");

$lb->setName("xxx_tab1.out");
$v = $lb->getName();
ok($v, "xxx_tab1.out");

$v = $lb->same($lb);
ok($v);

$v = $lb->same($t1->getInputLabel());
ok(!$v);

########################## chaining #################################################

@chain = $lb->getChain();
ok($#chain, -1);

# technically, chaining the input label of a table to its output label is a tight loop
# but here it doesn't matter
$res = $lb->chain($t1->getInputLabel());
ok($res);
ok($! . "", "");

@chain = $lb->getChain();
ok(join(", ", map {$_->getName()} @chain), "tab1.in");
$v = $t1->getInputLabel()->same($chain[0]);
ok($v);

# yes, the same chaining can be repeated!
$res = $lb->chain($t1->getInputLabel());
ok($res);
ok($! . "", "");

@chain = $lb->getChain();
ok(join(", ", map {$_->getName()} @chain), "tab1.in, tab1.in");

# incorrect chaining
$res = $lb->chain($lb);
ok(!$res);
ok($! . "", "Triceps::Label::chain: labels must not be chained in a loop\n  xxx_tab1.out->xxx_tab1.out");

# see that it's unchanged
@chain = $lb->getChain();
ok(join(", ", map {$_->getName()} @chain), "tab1.in, tab1.in");

# clear the chaining
$lb->clearChained();
@chain = $lb->getChain();
ok($#chain, -1);

######################### makeRowop ###################################
# tested in Rowop.t