//
//  comRadiaListViewController.m
//  ABradio
//
//  Created by Jan Damek on /194/12.
//  Copyright (c) 2012 droidsoft.eu. All rights reserved.
//

#import "comRadiaListViewController.h"
#import "AsyncImageView.h"
#import "comPlayerViewController.h"
#import "comAppDelegate.h"


@implementation comRadiaListViewController

@synthesize btnHraje;

-(unsigned int)radioList{
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    return delegate.radioList;
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    self.tableView.tableFooterView = [comAppDelegate getBanner];
    [[super tableView] reloadData];
    
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    
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

-(void)viewDidAppear:(BOOL)animated{
    [super viewDidAppear:animated];
    
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    
    if (delegate.radioList!=-1){
        NSMutableDictionary *item = [delegate.categoryAktual objectAtIndex:delegate.radioList-1];
        self.navigationItem.title = [item valueForKey:@"title"];
    }else{
        self.navigationItem.title = NSLocalizedString(@"allcat", nil);
    }
    
}

-(void)viewWillDisappear:(BOOL)animated{
    [super viewWillDisappear:animated];
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication]delegate];
    delegate.table = nil;
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    if (delegate.clicked){
        if ([self radioList]!=-1)
        {
            return 1;
        } else {
            return ceil([delegate.categoryAktual count] / 2);
        }
    }else return 0;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    if (delegate.clicked){
        if ([self radioList]!=-1){
            NSMutableArray *rl = [delegate.categoryAktual objectAtIndex:[self radioList]];
            return ceil([rl count]/2);
        } else {
            NSMutableArray *radioList = [delegate.categoryAktual objectAtIndex:(section*2)+1];
            return ceil([radioList count] / 2);
        }
    }else{
        return 0;
    }
}

- (NSString *)tableView:(UITableView *)tableView titleForHeaderInSection:(NSInteger)section
{
    if ([self radioList]==-1){
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
	NSMutableDictionary *item = [delegate.categoryAktual objectAtIndex:section*2];
    return [item valueForKey:@"title"];
    }else return @"";
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    static NSString *CellIdentifier = @"RadioListCell";
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier forIndexPath:indexPath];

    NSString *lo;
    if ([[UIDevice currentDevice] userInterfaceIdiom] == UIUserInterfaceIdiomPhone) {
        lo = @"logo";
    } else lo = @"logo_ipad";
    
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    if ([self radioList]!=-1){
        NSMutableArray *rl = [delegate.categoryAktual objectAtIndex:([self radioList])];
        NSMutableDictionary *item = [rl objectAtIndex:indexPath.row*2];
        cell.textLabel.text = [item valueForKey:@"name"];
        
        UIImage *image = [[[AsyncImageView alloc]init]getImage:[item valueForKey:lo] list:delegate.imageList imageView:cell.imageView cell:cell];
        cell.imageView.image = image;
        cell.detailTextLabel.text = [item valueForKey:@"description"];
    } else {
        NSInteger row = [indexPath row];
        NSInteger section = [indexPath section];
        NSMutableArray *radioList = [delegate.categoryAktual objectAtIndex:(section*2)+1];
        
        NSMutableDictionary *item = [radioList objectAtIndex:row*2];
        cell.textLabel.text = [item valueForKey:@"name"];
        
        UIImage *image = [[[AsyncImageView alloc]init]getImage:[item valueForKey:lo] list:delegate.imageList imageView:cell.imageView cell:cell];
        cell.imageView.image = image;
        cell.detailTextLabel.text = [item valueForKey:@"description"];
    }
    
    return cell;
}

#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    if ([self radioList]!=-1){
        NSMutableArray *rl = [delegate.categoryAktual objectAtIndex:([self radioList])];
        
        NSMutableDictionary *dataInt = [rl objectAtIndex:indexPath.row*2];
        NSMutableArray *stream = [rl objectAtIndex:(indexPath.row*2)+1];
        
        delegate.dataPlayer = dataInt;
        delegate.streamPlayer = stream;
    } else {
        NSMutableArray *radioList = [delegate.categoryAktual objectAtIndex:(indexPath.section*2)+1];
        NSMutableDictionary *dataInt = [radioList objectAtIndex:indexPath.row*2];
        NSMutableArray *stream = [radioList objectAtIndex:(indexPath.row*2)+1];
        
        delegate.dataPlayer = dataInt;
        delegate.streamPlayer = stream;
    }
    
    [delegate playStart];
}
//- (UIStatusBarStyle)preferredStatusBarStyle
//{
//    return UIStatusBarStyleLightContent;
//}


@end
