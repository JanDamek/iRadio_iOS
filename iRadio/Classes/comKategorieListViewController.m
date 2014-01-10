//
//  comKategorieListViewController.m
//  ABradio
//
//  Created by Jan Damek on /194/12.
//  Copyright (c) 2012 droidsoft.eu. All rights reserved.
//

#import "comKategorieListViewController.h"
#import "comRadiaListViewController.h"
#import "GAI.h"
#import "comPlayerViewController.h"
#import "comAppDelegate.h"


@implementation comKategorieListViewController

@synthesize sBar, btnHraje;

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];
    
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
    delegate.clicked = NO;
}

-(void)viewDidDisappear:(BOOL)animated{
    [super viewDidDisappear:animated];
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication]delegate];
    delegate.table = nil;
}

- (void) viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    self.tableView.tableFooterView = [comAppDelegate getBanner];
    [self.tableView reloadData];
    
//    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication]delegate];
//    [delegate performSelectorInBackground:@selector(parseXMLCategorie) withObject:nil];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    self.tableView.tableHeaderView = sBar;
    
    sBar.delegate = self;
    
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    delegate.table = self.tableView;    
    
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView
{
    return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section
{
    // Return the number of rows in the section.
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    [self.view setNeedsDisplay];
    return ceil([delegate.categoryAktual count]/2)+1;
}

- (UITableViewCell *)tableView:(UITableView *)tableView cellForRowAtIndexPath:(NSIndexPath *)indexPath
{
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    
    static NSString *CellIdentifier = @"CategoryCell";
    UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:CellIdentifier forIndexPath:indexPath];
    
    // Configure the cell...
    if (indexPath.row<([delegate.categoryAktual count]/2)){
        NSMutableDictionary *item = [delegate.categoryAktual objectAtIndex:indexPath.row*2];
        cell.textLabel.text = [item valueForKey:@"title"];
    }else {
        cell.textLabel.text = NSLocalizedString(@"allcat", nil);
    }
    
    return cell;
}


#pragma mark - Table view delegate

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath
{
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    NSInteger section = [indexPath row];
    NSInteger pocKat = [self tableView:tableView numberOfRowsInSection:indexPath.section];
    if (section < pocKat-1)
    {
        delegate.radioList = (section*2)+1;
    } else
        delegate.radioList = -1;
    
    delegate.clicked = YES;
    [delegate.table reloadData];
}

#pragma mark UISearchBarDelegate
- (void)searchBarTextDidBeginEditing:(UISearchBar *)searchBar
{
    searchBar.showsCancelButton = YES;
    searchBar.autocorrectionType = UITextAutocorrectionTypeDefault;
}

- (void)searchBarTextDidEndEditing:(UISearchBar *)searchBar
{
    searchBar.showsCancelButton = NO;
}

- (void)searchBarCancelButtonClicked:(UISearchBar *)searchBar
{
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    [delegate cancelSearch];
    [[super tableView] reloadData];
    
    [searchBar setText:@""];
    searchBar.showsCancelButton = NO;
    [searchBar resignFirstResponder];
}

- (void)searchBarSearchButtonClicked:(UISearchBar *)searchBar
{
    comAppDelegate *delegate = (comAppDelegate*)[[UIApplication sharedApplication] delegate];
    
    NSString *pageviewName = @"Search:";    
    [delegate.tracker set:pageviewName value:searchBar.text];

    [delegate setSearch:searchBar.text];
    [searchBar resignFirstResponder];
    [[super tableView] reloadData];
}
//- (UIStatusBarStyle)preferredStatusBarStyle
//{
//    return UIStatusBarStyleLightContent;
//}
//

@end
