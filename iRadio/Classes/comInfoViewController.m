//
//  comInfoViewController.m
//  ABradio
//
//  Created by Jan Damek on 26.10.13.
//  Copyright (c) 2013 droidsoft.eu. All rights reserved.
//

#import "comInfoViewController.h"

@interface comInfoViewController ()

@end

@implementation comInfoViewController

- (id)initWithNibName:(NSString *)nibNameOrNil bundle:(NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];
    if (self) {
        // Custom initialization
    }
    return self;
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

-(IBAction)closeInfoBtn:(id)sender
{
    [self dismissViewControllerAnimated:YES completion:nil];
}

-(void)btnDigitalScope:(id)sender{
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:@"http://www.digitalscope.cz"]];
}

-(void)btnMMS:(id)sender{
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:@"http://www.mms.cz"]];
}

-(void)btnNapisteNam:(id)sender{
        [[UIApplication sharedApplication] openURL:[NSURL URLWithString:[NSString stringWithFormat:@"mailto:%@" , @"app@abradio.cz"]]];
}


@end
