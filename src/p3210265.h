typedef unsigned int uint;

// how many cooks we have
uint NCook = 2;
// how many ovens we have
uint NOven = 15;
// how many packers we have
uint NPacker = 2;
// how many delivery men we have
uint NDeliverer = 10;
// lowest time it takes for a customer to get uinto the ordering system
uint TOrderlow = 1;
// highest time it takes for a customer to get uinto the ordering system
uint TOrderhigh = 3;
// lowest amount of pizzas in a single order
uint NOrderlow = 1;
// highest amount of pizzas in a single order
uint NOrderhigh = 5;
// possibility of a pizza being plain
uint PPlain = 60;
// lowest amount of time it takes for the ordering system to charge the customer
uint TPaymentlow = 1;
// highest amount of time it takes for the ordering system to charge the customer
uint TPaymenthigh = 3;
// possibility of the order failing due to failure in payment
uint PFail = 10;
// cost of plain pizza
uint CPlain = 10;
// cost of special pizza
uint CSpecial = 12;
// time it takes for a cook to make the pizza
uint TPrep = 1;
// time it takes for the oven to bake the pizza
uint TBake = 10;
// time it takes to pack each pizza
uint TPack = 1;
// lowest amount of time it takes for the delivery guy to deliver an order
uint TDellow = 5;
// highest amount of time it takes for the delivery guy to deliver an order
uint TDelhigh = 15;
