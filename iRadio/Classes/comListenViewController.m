//
//  comListenViewController.m
//  ABradio
//
//  Created by Jan Damek on /214/12.
//  Copyright (c) 2012 droidsoft.eu. All rights reserved.
//

#import "comListenViewController.h"
#import "comPlayerViewController.h"
#import "AsyncImageView.h"
#import "comAppDelegate.h"


@implementation comListenViewController

@synthesize btnHraje;

- (void)viewWillAppear:(BOOL)animated
{
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    
    [super viewWillAppear:animated];
    self.tableView.tableFooterView = [comAppDelegate getBanner];
    data = [delegate getPoslouchaneRadia];
    [[super tableView]reloadData];
    
    if (delegate.rate != 0.0) {
        if ([[super navigationItem] rightBarButtonItem] == nil) {
            self.navigationItem.rightBarButtonItem = btnHraje;
        }
    }
    else
    {
        if ([[super navigationItem] rightBarButtonItem] != nil) {
            [[super navigationItem] rightBarButtonItem];
            self.navigationItem.rightBarButtonItem = nil;
        }
        
    }
    delegate.table = self.tableView;
}

-(void)viewDidDisappear:(BOOL)animated{
    [super viewDidDisappear:animated];
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication]delegate];
    delegate.table = nil;
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    // Return the number of sections.
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    // Return the number of rows in the section.
    return ceil([data count]/2);
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString *CellIdentifier = @"ListenedCell";
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier forIndexPath:indexPath];
    
    NSMutableDictionary *item = [data objectAtIndex:indexPath.row*2];
    cell.textLabel.text = [item valueForKey:@"name"];
    
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    
    NSString *lo;
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
        lo = @"logo";
    } else lo = @"logo_ipad";
    UIImage *image = [[[AsyncImageView alloc]init]getImage:[item valueForKey:lo] list:delegate.imageList imageView:cell.imageView cell:cell];
    cell.imageView.image = image;
    cell.detailTextLabel.text = [item valueForKey:@"description"];    
    
    return cell;
}

- (void)tableView:(UITableView *)tableView commitEditingStyle:(UITableViewCellEditingStyle)editingStyle forRowAtIndexPath:(NSIndexPath *)indexPath
 {
     if (editingStyle == UITableViewCellEditingStyleDelete) {
         // Delete the row from the data source
         
         comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
         
         NSMutableDictionary *item = [data objectAtIndex:indexPath.row*2];
         
         [data removeObjectAtIndex:(indexPath.row*2)+1];
         [data removeObjectAtIndex:indexPath.row*2];
         
         for (int i=0; [[delegate poslouchane]count]>i; i++) {
             NSString *obl = [[delegate poslouchane] objectAtIndex:i];
             if ([obl isEqualToString:[item valueForKey:@"id"]]){
                 [[delegate poslouchane] removeObjectAtIndex:i];
             }
         }
         
         [tableView deleteRowsAtIndexPaths:[NSArray arrayWithObject:indexPath] withRowAnimation:UITableViewRowAnimationFade];
     }   
     else if (editingStyle == UITableViewCellEditingStyleInsert) {
         // Create a new instance of the appropriate class, insert it into the array, and add a new row to the table view
     }     
 }

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{    
    NSMutableDictionary *dataInt = [data objectAtIndex:indexPath.row*2];
    NSMutableArray *stream = [data objectAtIndex:(indexPath.row*2)+1];
    
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    
    delegate.dataPlayer = dataInt;
    delegate.streamPlayer = stream;
    
    [delegate playStart];
}


@end
