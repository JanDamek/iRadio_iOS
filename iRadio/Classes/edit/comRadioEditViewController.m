//
//  comRadioEditViewController.m
//  iRadioHD
//
//  Created by Jan Damek on 10.03.14.
//  Copyright (c) 2014 Jan Damek. All rights reserved.
//

#import "comRadioEditViewController.h"
#import "comAppDelegate.h"

@interface comRadioEditViewController ()

@end

@implementation comRadioEditViewController

@synthesize radio = _radio;

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
}

- (IBAction)saveAction:(UIBarButtonItem *)sender {
    comAppDelegate *d = (comAppDelegate*)[[UIApplication sharedApplication]delegate];
    [d.data saveCategories];
    
    [self dismissViewControllerAnimated:YES completion:nil];
    [self.popover dismissPopoverAnimated:YES];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
	// Do any additional setup after loading the view.
}

- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}

@end
