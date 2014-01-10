//
//  comContactViewController.m
//  ABradio
//
//  Created by Jan Damek on /36/12.
//  Copyright (c) 2012 droidsoft.eu. All rights reserved.
//

#import "comContactViewController.h"

@interface comContactViewController ()

@end

@implementation comContactViewController

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

- (void)viewDidUnload
{
    [super viewDidUnload];
    // Release any retained subviews of the main view.
}

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation
{
    return (interfaceOrientation == UIInterfaceOrientationPortrait);
}

-(IBAction)zpetBtn:(id)sender;
{
//    [self dismissModalViewControllerAnimated:YES];
    [self dismissViewControllerAnimated:YES completion:nil];
}

-(IBAction)limeBtn:(id)sender;
{
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:@"http://www.limemedia.cz/"]];    
}

-(IBAction)digitalBtn:(id)sender;
{
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:@"http://www.digitalscope.cz/"]];    
}

-(IBAction)mailBtn:(id)sender;
{
    [[UIApplication sharedApplication] openURL:[NSURL URLWithString:@"mailto:app@abradio.cz"]];     
}
//- (UIStatusBarStyle)preferredStatusBarStyle
//{
//    return UIStatusBarStyleLightContent;
//}


@end
